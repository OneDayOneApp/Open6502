#include "nes/ref/nes_ref.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int nes_ref_rom_load_mem(nes_ref_rom *r, const uint8_t *data, size_t size) {
  if (r == NULL || data == NULL) return NES_REF_ERR_INVALID;
  memset(r, 0, sizeof(*r));
  if (size < 16 || memcmp(data, "NES\x1A", 4) != 0) return NES_REF_ERR_FORMAT;

  const uint8_t *h = data;
  int trainer = (h[6] & 0x04) != 0;
  size_t header = 16 + (trainer ? 512 : 0);
  size_t prg_size = (size_t)h[4] * 16384u;
  size_t chr_size = (size_t)h[5] * 8192u;
  int mapper = ((h[6] >> 4) & 0x0F) | (h[7] & 0xF0);
  bool chr_ram = chr_size == 0;

  if (prg_size == 0 || size < header + prg_size + chr_size) {
    return NES_REF_ERR_FORMAT;
  }
  if (chr_ram) chr_size = 8192;

  r->prg = malloc(prg_size ? prg_size : 1);
  r->chr = calloc(1, chr_size ? chr_size : 1);
  if (r->prg == NULL || r->chr == NULL) {
    nes_ref_rom_free(r);
    return NES_REF_ERR_NOMEM;
  }

  memcpy(r->prg, data + header, (size_t)h[4] * 16384u);
  if (!chr_ram) memcpy(r->chr, data + header + (size_t)h[4] * 16384u, chr_size);

  r->mapper = mapper;
  r->prg_size = prg_size;
  r->chr_size = chr_size;
  r->chr_ram = chr_ram;
  r->battery = (h[6] & 0x02) != 0;
  r->four_screen = (h[6] & 0x08) != 0;
  r->mirroring = (h[6] & 0x01) ? 1 : 0;
  return NES_REF_OK;
}

int nes_ref_rom_load_file(nes_ref_rom *r, const char *path) {
  if (r == NULL || path == NULL) return NES_REF_ERR_INVALID;
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return NES_REF_ERR_INVALID;
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NES_REF_ERR_INVALID;
  }
  long len = ftell(fp);
  if (len < 0) {
    fclose(fp);
    return NES_REF_ERR_INVALID;
  }
  rewind(fp);
  uint8_t *buf = malloc((size_t)len ? (size_t)len : 1);
  if (buf == NULL) {
    fclose(fp);
    return NES_REF_ERR_NOMEM;
  }
  if (fread(buf, 1, (size_t)len, fp) != (size_t)len) {
    free(buf);
    fclose(fp);
    return NES_REF_ERR_INVALID;
  }
  fclose(fp);
  int ret = nes_ref_rom_load_mem(r, buf, (size_t)len);
  free(buf);
  return ret;
}

void nes_ref_rom_free(nes_ref_rom *r) {
  if (r == NULL) return;
  free(r->prg);
  free(r->chr);
  memset(r, 0, sizeof(*r));
}
