# DiPDF

DiPDF là ứng dụng đọc PDF viết bằng **C++ / Qt 6**. App dùng **Poppler Qt6** để render PDF và đóng gói Linux bằng **AppImage**.

## Tính năng chính

- Mở và đọc file PDF.
- Giao diện Qt Widgets/QML.
- Icon và font được nhúng qua `resources.qrc`.
- Đóng gói thành AppImage để chạy trực tiếp trên Linux.
- Trên Linux, app mặc định ép `QT_QPA_PLATFORM=xcb` để tránh lỗi mất nút đóng/thu nhỏ/phóng to khi chạy AppImage trên Wayland.

## Yêu cầu hệ thống

### Runtime

Để chạy AppImage, máy Linux cần có FUSE. Trên Fedora:

```bash
sudo dnf install -y fuse fuse-libs
```

Trên Ubuntu/Debian:

```bash
sudo apt install -y libfuse2t64 || sudo apt install -y libfuse2
```

## Chạy AppImage

Sau khi tải hoặc build được file `.AppImage`:

```bash
chmod +x DiPDF-*.AppImage
./DiPDF-*.AppImage
```

Nếu muốn ép chạy bằng XCB thủ công:

```bash
QT_QPA_PLATFORM=xcb ./DiPDF-*.AppImage
```

Nếu muốn giữ platform mặc định của hệ thống thay vì ép XCB:

```bash
DIPDF_KEEP_QPA_PLATFORM=1 ./DiPDF-*.AppImage
```

## Build native trên Linux

### Fedora

```bash
sudo dnf install -y \
  gcc-c++ cmake ninja-build git wget file patchelf desktop-file-utils \
  pkgconf-pkg-config \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel qt6-qtwayland-devel \
  poppler-qt6-devel \
  libxkbcommon-devel libxkbcommon-x11-devel xcb-util-cursor-devel \
  fuse fuse-libs
```

Build app:

```bash
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build build
./build/DiPDF
```

### Ubuntu 24.04

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake ninja-build git wget file patchelf desktop-file-utils \
  ca-certificates pkg-config libfuse2t64 \
  qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-svg-dev qt6-wayland \
  libpoppler-qt6-dev \
  libxkbcommon-dev libxkbcommon-x11-dev \
  libxcb-cursor0 libxcb-cursor-dev
```

Build app:

```bash
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr

cmake --build build
./build/DiPDF
```

## Build AppImage bằng Docker

Cách này được khuyến nghị nếu máy host là Fedora bản mới, vì build trực tiếp trên Fedora mới có thể bundle quá nhiều thư viện hệ thống mới vào AppImage.

### 1. Tạo Docker image build

File `Dockerfile.appimage` nên có nội dung tương tự:

```dockerfile
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV APPIMAGE_EXTRACT_AND_RUN=1

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    file \
    patchelf \
    desktop-file-utils \
    ca-certificates \
    pkg-config \
    libfuse2t64 \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-declarative-dev \
    qt6-svg-dev \
    qt6-wayland \
    libpoppler-qt6-dev \
    libxcb-cursor0 \
    libxcb-cursor-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-0 \
    libxkbcommon-x11-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
```

Build image:

```bash
docker build -f Dockerfile.appimage -t dipdf-appimage-builder .
```

### 2. Build AppImage trong Docker

Repo cần có script `build-appimage-docker.sh`.

Chạy trên Fedora có SELinux:

```bash
docker run --rm -it \
  -v "$PWD":/work:Z \
  -w /work \
  dipdf-appimage-builder \
  ./build-appimage-docker.sh
```

Trên Ubuntu/Debian host, nếu `:Z` gây lỗi thì bỏ `:Z`:

```bash
docker run --rm -it \
  -v "$PWD":/work \
  -w /work \
  dipdf-appimage-builder \
  ./build-appimage-docker.sh
```

Nếu file build ra bị owner là `root`, sửa lại quyền:

```bash
sudo chown -R "$USER:$USER" .
```

Sau khi build xong:

```bash
ls -lh *.AppImage
chmod +x DiPDF-*.AppImage
./DiPDF-*.AppImage
```

## Build AppImage bằng GitHub Actions

Workflow CI nằm tại:

```text
.github/workflows/build-appimage.yml
```

Khi push code, mở pull request, chạy thủ công bằng `workflow_dispatch`, hoặc tạo tag dạng `v*`, GitHub Actions sẽ:

1. Cài Qt6, Poppler Qt6 và các dependency cần thiết trên Ubuntu 24.04.
2. Chạy `build-appimage-docker.sh` để build AppImage.
3. Upload file `.AppImage` vào workflow artifacts.
4. Nếu build từ tag `v*`, upload thêm AppImage vào GitHub Release.

## Ghi chú kỹ thuật

### Vì sao app ép `QT_QPA_PLATFORM=xcb`?

Trên một số desktop Wayland, Qt AppImage có thể chạy không có server-side decoration, làm mất thanh cửa sổ native và mất nút đóng/thu nhỏ/phóng to. Vì vậy app ép `xcb` trước khi tạo `QApplication`.

Có thể override bằng:

```bash
DIPDF_KEEP_QPA_PLATFORM=1 ./DiPDF-*.AppImage
```

### Vì sao icon phải nằm trong `resources.qrc`?

Khi chạy trong AppImage, working directory không còn là thư mục source project. Vì vậy path kiểu `assets/icon.svg` dễ bị mất. Icon/font nên dùng Qt resource path:

```cpp
QIcon(":/assets/icon.png")
QIcon(":/assets/home.svg")
```

Với SVG icon, project cần link thêm `Qt6::Svg` trong `CMakeLists.txt`.

## Dọn build

```bash
rm -rf build AppDir tools squashfs-root *.AppImage
```
