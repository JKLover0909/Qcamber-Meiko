# CLAUDE.md

Guidance for Claude Code (and similar agents) working in this repository.

## What this repo is

**QCamber** — trình xem file Gerber/ODB++ mã nguồn mở (C++/Qt), bản tùy biến nội bộ cho Meiko (thêm module `src/restapi/`). Đây **không phải code viết mới từ đầu** — là bản dựa trên [QCamber gốc](https://github.com/aitjcize/QCamber) của AZ Huang và cộng sự (xem `AUTHORS`).

## Giấy phép — quan trọng

- Repo này dùng **GNU GPLv3** (`COPYING`), không phải MIT như các repo cá nhân khác của user. **Không thêm file `LICENSE` (MIT) chồng lên** — `COPYING` đã là license hợp lệ, đổi sang license khác sẽ vi phạm điều khoản GPL của code gốc.
- Giữ nguyên `AUTHORS` khi sửa code — đây là ghi nhận tác giả gốc theo yêu cầu của GPL.

## Layout

```
src/
  parser/         # Gerber/ODB++ parsing
  graphicsview/    # render layer PCB
  gui/             # Qt UI
  codegen/         # xuất mã
  symbol/          # thư viện symbol
  restapi/         # REST API — phần tùy biến thêm cho Meiko, KHÔNG có trong QCamber gốc
  tests/
bin/, prebuilt/    # binary build sẵn (Windows)
```

## Lưu ý khi sửa

- `config.ini` (`RootDir=D:/New folder/Qcamber-Meiko/bin/Jobs`) và `qcamber.sln` (`C:/Users/sonng/Code/QCamber/src\...`) đang hard-code đường dẫn cá nhân của máy dev — không portable. Nếu sửa các file này, hỏi lại trước khi đổi vì có thể build hiện tại đang phụ thuộc vào đúng path đó.
- Đây là dự án C++/Qt/MSVC (`qcamber.sln`) — không có test suite Python để chạy nhanh; `src/tests/` là test C++ (Qt Test), cần build toàn bộ mới chạy được, không khả thi trong môi trường agent không có Qt/MSVC.
- README hiện tại chỉ ghi quy trình git nội bộ (không push thẳng vào `main`, PR cần 1 approval) — tuân theo quy trình đó khi đề xuất thay đổi.
