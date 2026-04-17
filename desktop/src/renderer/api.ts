import type { TokmagotchiApi } from "../preload";

declare global {
  interface Window {
    tokmagotchi: TokmagotchiApi;
  }
}

export const api = window.tokmagotchi;
