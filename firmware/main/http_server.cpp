#include "http_server.h"
#include "pet_state.h"
#include "display.h"
#include "constants.h"
#include "sd_storage.h"

#include <map>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <vector>
#include <cstring>

#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

namespace tokmagotchi {

static const char *TAG = "http_server";

namespace {

httpd_handle_t g_server = nullptr;

struct PendingPermission {
    std::mutex m;
    std::condition_variable cv;
    bool resolved = false;
    bool allow = false;
};

std::mutex g_perm_mu;
std::map<std::string, std::shared_ptr<PendingPermission>> g_pending;

esp_err_t send_json(httpd_req_t *req, cJSON *json, int status = 200) {
    char *out = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    if (status != 200) {
        char status_buf[16];
        snprintf(status_buf, sizeof(status_buf), "%d", status);
        httpd_resp_set_status(req, status_buf);
    }
    esp_err_t r = httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
    free(out);
    cJSON_Delete(json);
    return r;
}

esp_err_t read_body(httpd_req_t *req, std::string &out) {
    int total = req->content_len;
    if (total <= 0 || total > 8192) return ESP_FAIL;
    out.resize(total);
    int received = 0;
    while (received < total) {
        int n = httpd_req_recv(req, out.data() + received, total - received);
        if (n <= 0) return ESP_FAIL;
        received += n;
    }
    return ESP_OK;
}

esp_err_t handle_feed(httpd_req_t *req) {
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body required");
        return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(j, "type"));
    const char *size = cJSON_GetStringValue(cJSON_GetObjectItem(j, "size"));
    cJSON *tok = cJSON_GetObjectItem(j, "tokens");
    int tokens = tok && cJSON_IsNumber(tok) ? (int)tok->valuedouble : 0;

    if (type && std::strcmp(type, "token") == 0) {
        bool large = size && std::strcmp(size, "large") == 0;
        PetState::instance().feed_token(large, tokens);
    } else if (type && std::strcmp(type, "voice") == 0) {
        PetState::instance().feed_voice();
    } else if (type && std::strcmp(type, "vision") == 0) {
        PetState::instance().feed_vision();
    } else {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "unknown feed type");
        return ESP_FAIL;
    }
    cJSON_Delete(j);
    PetState::instance().save();

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "ok", "true");
    return send_json(req, resp);
}

esp_err_t handle_permission(httpd_req_t *req) {
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body required");
        return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid json");
        return ESP_FAIL;
    }
    PermissionDisplayRequest dr;
    const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(j, "id"));
    const char *tool = cJSON_GetStringValue(cJSON_GetObjectItem(j, "tool"));
    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(j, "path"));
    const char *cmd  = cJSON_GetStringValue(cJSON_GetObjectItem(j, "command"));
    if (!id || !tool) {
        cJSON_Delete(j);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "id+tool required");
        return ESP_FAIL;
    }
    dr.id = id;
    dr.tool = tool;
    if (path) dr.path = path;
    if (cmd)  dr.command = cmd;
    cJSON_Delete(j);

    auto slot = std::make_shared<PendingPermission>();
    {
        std::lock_guard<std::mutex> lk(g_perm_mu);
        g_pending[dr.id] = slot;
    }

    display_permission_show(dr);

    bool allow = false;
    bool timed_out = false;
    {
        std::unique_lock<std::mutex> lk(slot->m);
        if (!slot->cv.wait_for(lk, std::chrono::milliseconds(PERMISSION_TIMEOUT_MS),
                               [&] { return slot->resolved; })) {
            timed_out = true;
        }
        allow = slot->resolved && slot->allow;
    }
    {
        std::lock_guard<std::mutex> lk(g_perm_mu);
        g_pending.erase(dr.id);
    }

    if (timed_out) {
        display_permission_clear();
        PetState::instance().on_permission_resolved(false);
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "id", dr.id.c_str());
    cJSON_AddStringToObject(resp, "decision", allow ? "allow" : "deny");
    cJSON_AddStringToObject(resp, "decidedBy", timed_out ? "timeout" : "device");
    return send_json(req, resp);
}

esp_err_t handle_state(httpd_req_t *req) {
    auto snap = PetState::instance().snapshot();
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "state", PetState::mood_label(snap.mood));
    cJSON_AddStringToObject(resp, "emoji", PetState::instance().current_emoji());
    cJSON_AddStringToObject(resp, "name",  snap.name.c_str());
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "tokens", snap.meters.tokens);
    cJSON_AddNumberToObject(m, "voice",  snap.meters.voice);
    cJSON_AddNumberToObject(m, "vision", snap.meters.vision);
    cJSON_AddItemToObject(resp, "meters", m);
    cJSON *lf = cJSON_CreateObject();
    cJSON_AddNumberToObject(lf, "tokens", snap.last_token_feed);
    cJSON_AddNumberToObject(lf, "voice",  snap.last_voice_feed);
    cJSON_AddNumberToObject(lf, "vision", snap.last_vision_feed);
    cJSON_AddItemToObject(resp, "lastFeed", lf);
    return send_json(req, resp);
}

esp_err_t handle_media_list(httpd_req_t *req) {
    cJSON *resp = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    for (const auto &f : sd_storage_list()) {
        cJSON *fi = cJSON_CreateObject();
        cJSON_AddStringToObject(fi, "name", f.name.c_str());
        cJSON_AddNumberToObject(fi, "size", f.size);
        cJSON_AddStringToObject(fi, "type", f.type.c_str());
        cJSON_AddItemToArray(files, fi);
    }
    cJSON_AddItemToObject(resp, "files", files);
    return send_json(req, resp);
}

esp_err_t handle_media_file(httpd_req_t *req) {
    const char *uri = req->uri;
    const char *slash = std::strrchr(uri, '/');
    if (!slash || !*(slash + 1)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "filename required");
        return ESP_FAIL;
    }
    std::string name = slash + 1;
    if (name.find("..") != std::string::npos) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid name");
        return ESP_FAIL;
    }
    std::string path = std::string(SD_MOUNT_POINT) + "/" + name;
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "no such file");
        return ESP_FAIL;
    }
    if (name.find(".wav") != std::string::npos) {
        httpd_resp_set_type(req, "audio/wav");
    } else if (name.find(".jpg") != std::string::npos) {
        httpd_resp_set_type(req, "image/jpeg");
    } else {
        httpd_resp_set_type(req, "application/octet-stream");
    }
    char buf[1024];
    while (true) {
        size_t n = std::fread(buf, 1, sizeof(buf), f);
        if (n == 0) break;
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            std::fclose(f);
            return ESP_FAIL;
        }
    }
    std::fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

esp_err_t handle_name(httpd_req_t *req) {
    std::string body;
    if (read_body(req, body) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body required");
        return ESP_FAIL;
    }
    cJSON *j = cJSON_Parse(body.c_str());
    if (!j) return ESP_FAIL;
    const char *n = cJSON_GetStringValue(cJSON_GetObjectItem(j, "name"));
    if (n && *n) PetState::instance().set_name(n);
    cJSON_Delete(j);
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "name", PetState::instance().name().c_str());
    return send_json(req, resp);
}

} // namespace

void http_server_start() {
    if (g_server) return;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = DEVICE_HTTP_PORT;
    cfg.max_uri_handlers = 12;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&g_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "failed to start http server");
        g_server = nullptr;
        return;
    }

    httpd_uri_t u_feed       = {"/feed",       HTTP_POST, handle_feed,       nullptr};
    httpd_uri_t u_permission = {"/permission", HTTP_POST, handle_permission, nullptr};
    httpd_uri_t u_state      = {"/state",      HTTP_GET,  handle_state,      nullptr};
    httpd_uri_t u_media_list = {"/media/list", HTTP_GET,  handle_media_list, nullptr};
    httpd_uri_t u_media_file = {"/media/*",    HTTP_GET,  handle_media_file, nullptr};
    httpd_uri_t u_name       = {"/name",       HTTP_POST, handle_name,       nullptr};

    httpd_register_uri_handler(g_server, &u_feed);
    httpd_register_uri_handler(g_server, &u_permission);
    httpd_register_uri_handler(g_server, &u_state);
    httpd_register_uri_handler(g_server, &u_media_list);
    httpd_register_uri_handler(g_server, &u_media_file);
    httpd_register_uri_handler(g_server, &u_name);

    display_set_permission_decision_cb(&http_server_resolve_permission);
    ESP_LOGI(TAG, "http server listening on :%d", DEVICE_HTTP_PORT);
}

void http_server_stop() {
    if (!g_server) return;
    httpd_stop(g_server);
    g_server = nullptr;
}

void http_server_resolve_permission(const std::string &id, bool allow) {
    std::shared_ptr<PendingPermission> slot;
    {
        std::lock_guard<std::mutex> lk(g_perm_mu);
        auto it = g_pending.find(id);
        if (it == g_pending.end()) return;
        slot = it->second;
    }
    {
        std::lock_guard<std::mutex> lk(slot->m);
        slot->resolved = true;
        slot->allow = allow;
    }
    slot->cv.notify_all();
}

} // namespace tokmagotchi
