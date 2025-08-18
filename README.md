# Đồ án: Xây dựng phần mềm điều khiển PC từ xa qua Email
**Nhóm 3 - 24C06**  
**Thành viên:**
- Lê Minh Đức - 24127025
- Đoàn Quốc Bảo - 24127326
- Lê Trần Quang Huy - 24127396

**Tổng quan**
- Phần mềm điều khiển một hoặc nhiều máy qua mạng LAN từ một máy chạy server. Có thể tương tác trực tiếp với giao diện hoặc gửi yêu cầu qua Email (cụ thể là Gmail)


**Các chức năng**
1. List / Start / Stop tiến trình đang chạy trong máy
2. Start / Stop ứng dụng
3. Chụp toàn bộ màn hình
4. Bắt phím nhấn
5. Start / Stop Webcam để lấy đoạn clip mà webcam quay
6. Liệt kê / Lấy files
7. Reset / Shutdown máy
8. Gửi tin nhắn hiển thị trên màn hình

## Yêu cầu
- Một máy tính Windows sử dụng làm server
- Một hoặc nhiều máy tính Windows sử dụng làm client bị điều khiển

## Các thư viện sử dụng
- [libsodium](https://github.com/jedisct1/libsodium) để mã hóa gói tin (đã có kèm theo)
- [cURL](https://github.com/curl/curl) để tương tác REST API với Gmail trong việc gửi nhận mail (đã có kèm theo)
- [Dear ImGui](https://github.com/ocornut/imgui) để làm giao diện đồ họa (đã có kèm theo)
- [nlohmann/json](https://github.com/nlohmann/json) để đọc ghi dữ liệu qua định dạng JSON (đã có kèm theo)

## Build
### 0. Yêu cầu toolchain
- Compiler MinGW-gcc hỗ trợ C++17
- CMake 3.10 hoặc mới hơn (và cần một build system để build, đề xuất là Ninja)
- Python 3.8 hoặc mới hơn

### 1. Setup Gmail API và Google OAuth
- Tạo và lấy Client ID và Client secret của ứng dụng trên Google Cloud Console
- Ở mục `Authorized redirect URIs`, đặt một URL có đường dẫn là `http://localhost:62397`. Đây chính là URL callback khi chúng ta mở một server HTTP để nhân credential trong quá trình thiết lập OAuth
- Chỉnh sửa file `credential.json` với nội dung như sau
```
{
    "clientId": "<client-id-cua-ung-dung>",
    "clientSecret": "<client-secret-cua-ung-dung>"
}
```
- Nếu không thể để port mặc định là 62397, đổi thành port khác trên đường dẫn, sau đó cập nhật lại file `credential.json`
```
{
    "clientId": "<client-id-cua-ung-dung>",
    "clientSecret": "<client-secret-cua-ung-dung>"
    "redirectPort": "<port-khac>",
}
```

### 2. Build thư viện ImGui
- Đi tới thư mục `imgui-win32-dx9`, tạo folder build và khởi tạo cmake
```
cd imgui-win32-dx9 && mkdir build && cd build
```
- Config CMake & build
```
cmake ..
cmake --build .
```

### 3. Build toàn bộ mã nguồn
- Tại thư mục gốc, đi đến thư mục `build-config` và chạy file `build.py`
```
python build.py --release [-j <số-luồng>]
```
- Mặc định số luồng sử dụng để build bằng với số luồng của máy. Có thể chỉnh qua parameter `-j`
- Sau khi build, các file exe năm ở thư mục `bin` trên cây thư mục chính
- Tại đây tất cả các file cần của server đã được sao chép đến để có thể chạy
- Dữ liệu qua tương tác của server năm ở thư mục `server-tmp` trong thư mục `bin` trên cây thư mục chính
- Với exe của client có thể chạy độc lập ở bất kỳ đường dẫn nào và có thể phân phối đến các máy khác


## Cấu trúc mã nguồn

- Folder `include` chứa các header của chương trình
- Folder `src` chứa mã của chương trình, bao gồm các file
    + `client.cpp`: mã nguồn file client
    + `server.cpp`: mã nguồn file server
    + Folder `component` chứa những mã dùng chung cho client và server
    + Folder `engine` chứa mã cài đặt các chức năng khác nhau của client

## Tương tác qua Gmail
- Để gửi lệnh qua gmail, server cần được xác thực Gmail (không còn nút `Authorize Gmail` mà là nút `Reauthorize Gmail...`)
- Gửi mail với tiêu đề (subject) là `[PROJECT-MMT] <ip-và-port-máy-client>`
- Với phần thân (body) là lệnh cần gửi là parameter của lệnh
- IP và port của máy client cần gửi có thể thấy trên giao diện server (và có nút để copy vào clipboard)
- Các lệnh và parameter
    + MessageScreenCap
    + MessageEnableKeylog
    + MessageDisableKeylog
    + MessageInvokeWebcam <milli-giây> <[optional] fps>
    + MessageListFile <đường-dẫn-thư-mục>
    + MessageGetFile <đường-dẫn-file> 
    + MessageStartProcess <file-chương-trình> <các-parameters-của-chương-trình> ...
    + MessageStopProcess <pid-tiến-trình>
    + MessageListProcess 
    + MessageShutdownMachine
    + MessageRestartMachine
- Mail trả về sẽ có dịnh dạng subject là `[PROJECTMMT-REPLY] ..., trong body sẽ chứa file gửi về, hoặc thông báo lệnh sai, thực thi lỗi, ...
- Khi gửi mail với subject là `[PROJECT-MMT] ALL`, mail trả ngược về sẽ là địa chỉ IP và port các client đang được kết nối

Vd1: Mail của server là `testserver@gmail.com`. Cần lấy file `C:/data/Hải và Minh.png` từ client `127.0.0.1:55555`, ta soạn mail
```
From: mail-của-bạn
To: testserver@gmail.com
Subject: [PROJECT-MMT] 127.0.0.1:55555
Body: MessageGetFile "C:/data/Hải và Minh.png"
```
Mail trả về thành công
```
From: testserver@gmail.com
To: mail-của-bạn
Subject: [PROJECTMMT-REPLY] 127.0.0.1:55555
Body: <file Hải và Minh.png>
```
... hoặc mail thất bại
```
From: testserver@gmail.com
To: mail-của-bạn
Subject: [PROJECTMMT-REPLY] 127.0.0.1:55555
Body: Failed to get file
```

- Vì giới hạn limit của Gmail APi nên server sẽ đọc hợp thư đến mỗi 15 giây một lần nên mail gửi tới có thể mất một thời gian để xử lý

## Vấn đề/Giới hạn phần mềm
- Chỉ điều khiển qua mạng LAN (hoặc qua các mạng VPN nối các máy với nhau, vd Tailscale)
- Khi tương tác qua mail, chỉ các mail vào Hộp thư đến được xử lý (không xử lý trường hợp thư rác)
- Bug trong lập trình đa luồng: Khi có từ 2 máy trở lên kết nối và nhận lệnh từ mail sẽ có trường hợp hiếm khiến server bị crash 
- Khi file / dữ liệu có dấu Tiếng Việt, có thể bị lỗi encoding qua đó mail có thể hiển thị sai hoặc lệnh chưa được thực thi
