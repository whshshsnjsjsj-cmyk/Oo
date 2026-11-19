# CLEO_VisualObject + CLEOPlus (arm64, GTA SA 2.10)

Dua plugin AML pengganti versi 32-bit: `libCLEO_VisualObject.so` dan `libCLEOPlus.so`.
Script `.csa` kamu tidak perlu diubah.

## Cara build dari HP (tanpa PC)
1. Buka github.com di browser HP, login, lalu buat repository baru (boleh private).
2. Di repository: Add file > Upload files. Unggah 7 file ini: CMakeLists.txt, common.h, main.cpp, peds.cpp, plus.cpp, visual.cpp, README.md. Lalu Commit.
3. Add file > Create new file. Pada kolom nama ketik persis `.github/workflows/build.yml`, tempel isi file build.yml, lalu Commit.
4. Buka tab Actions > build-arm64. Kalau belum jalan sendiri, tekan Run workflow. Tunggu 3-5 menit sampai centang hijau.
5. Buka run tersebut, di bagian Artifacts unduh `plugins-arm64`. Ekstrak: isinya dua file `.so`.

## Cara pasang
1. Hapus `libCLEO_VisualObject.so` dan `libCLEOPlus.so` 32-bit lama kalau ada.
2. Taruh dua file `.so` baru di folder yang sama dengan mod AML 64-bit lainnya.
3. Script `.csa` dan folder ArtemQa tetap di tempatnya semula. Jalankan game.
4. Kalau gagal build atau game crash, kirim teks error dari Actions atau crash log-nya.

## Batasan versi 1
- Belum diuji di perangkat. Struktur API CLEO 64-bit (`cleo.h`) dan offset `CPed` ditebak dari build terbuka dan dikalibrasi saat jalan.
- `auto_hide` (6003) baru menyimpan flag: objek belum otomatis hilang saat ped mati, memegang senjata, atau di mobil.
- Partikel asap (0EC0) belum menampilkan apa pun. `is_char_script_controlled` (0E0A) selalu false.
- Penanda byte 0x11 pada struct ped (dipakai script Hats) ditiru di dalam plugin.
- Tekstur PNG (601F) memakai `RtPNGImageRead` milik game, dan bisa gagal bergantung pada cara game membaca file.
