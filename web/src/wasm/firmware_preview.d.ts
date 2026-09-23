export type FirmwarePreviewModule = {
  _preview_init(width: number, height: number, dpi: number): void;
  _preview_set_profile(columns: number, rows: number): void;
  _preview_set_climate(target: number, room: number, mode: string): void;
  _preview_render(): void;
  _preview_frame(): number;
  _preview_width(): number;
  _preview_height(): number;
  HEAPU8: Uint8Array;
};
const createModule: (options?: Record<string, unknown>) => Promise<FirmwarePreviewModule>;
export default createModule;
