# SmartOn PC (Waveshare ESP32-C6-GEEK Edition)

Waveshare **ESP32-C6-GEEK** を使用し、PC（Ubuntu 等）を **Matter** プロトコル対応スマートホームデバイスとして制御するファームウェアです。  
Google Home / Nest Hub / Apple Home 等の標準スマートホームエコシステムからの音声操作や自動化ルールによって PC をリモート起動（Wake-on-LAN）できます。

---

## ✨ 主な特徴

- **Windows Ubuntu両対応**:USBとWOLの組み合わせによりWindows/Ubuntu両OSで利用可能。
- **専用配線の完全廃止 (Wiring-Free)**: メカリレーやフォトカプラ等のマザーボード配線は一切不要。PC の USB ポートに刺すだけで導入可能。
- **Wake-on-LAN (WOL) リモート起動**: Matter から ON 操作を受信した際、設定された MAC アドレス宛てに Wake-on-LAN マジックパケットを送信して PC を起動。
- **USB 通信状態ベースの PC 監視**: 2秒周期で USB Serial/JTAG 接続状態（`usb_serial_jtag_is_connected()`）を監視し、PC の実態（ON / OFF）を判定。
- **Matter 属性自動同期 & フェイルセーフ**: PC の実態と Matter 属性値の乖離を検出した際、誤操作防止の安全ガード（`s_syncing_attribute`）付きで Matter 属性を自動更新・自動同期。
- **ST7789 カラー LCD リアルタイム表示**: Wi-Fi 接続状態、割り当て IP アドレス、PC 状態（ON / OFF / BOOTING）、動作ステータスログをグラフィカルに表示。
- **省電力＆画面保護（5分自動消灯・手動トグル）**: PC が `OFF` になってから 5 分経過すると LCD バックライト（輝度）を自動で `0`（消灯）に移行。PC 起動・復帰で自動点灯。また本体 BOOT ボタン（GPIO 9）短押しでいつでも手動 ON/OFF 切替が可能。
- **Google Home 互換性最適化**:
  - Wi-Fi 省電力モード無効化（`WIFI_PS_NONE`）による IPv6 mDNS（`_matter._tcp`）ドロップ防止。
  - MAC アドレスベースの決定論的 UniqueID（`chip-config/unique-id`）自動生成。

---

## 🛠️ ハードウェア仕様

- **マイコンボード**: Waveshare ESP32-C6-GEEK
- **ディスプレイ**: 1.14 インチ ST7789 カラー LCD (240 × 135)
- **接続方式**: USB Type-A 接続（PC の USB ポート）
- **物理配線**: なし（USB 給電および接続検知）
- **物理ボタン**: GPIO 9 (BOOT ボタン短押しで画面 ON/OFF / 長押し 3 秒で Factory Reset)


---

## 💻 ソフトウェアスタック

| レイヤー | 名称 | バージョン / 設定 |
|---------|------|------------------|
| OS / SDK | ESP-IDF | v5.4.1 |
| ターゲット | ESP32-C6 | `esp32c6` |
| スマートホーム規格 | esp-matter / Matter | Wi-Fi (`kOnNetwork`) |
| デバイスタイプ | On/Off Plug-in Unit | Cluster: `0x0006` (OnOff) |
| 電源制御方式 | Wake-on-LAN (WOL) | UDP Port 9 / Magic Packet |

---

## ⚙️ ネットワーク設定 (`main/wifi_creds.h`)

セキュリティ上の理由により、SSID やパスワード、MAC アドレス等の認証情報は Git 管理対象外（`.gitignore` に登録）となっています。  
初回ビルド前に `main/wifi_creds.h` を新規作成し、お使いの環境に合わせて以下の内容を設定してください。

### `main/wifi_creds.h` のテンプレート

```c
#pragma once

// Wi-Fi 接続情報
#define WIFI_SSID     "Your_WiFi_SSID"
#define WIFI_PASSWORD "Your_WiFi_Password"

// ターゲット PC 設定
#define PC_HOSTNAME   "your-pc-hostname"     // mDNSホスト名（.localなし）
#define PC_MAC_ADDRESS "XX:XX:XX:XX:XX:XX"  // WOL対象PCのMACアドレス

//#define PC_STATIC_IP  "192.168.11.13"     // オプション: mDNSをスキップして直接IP指定する場合

// 軽量シャットダウンデモ用ポート & トークン設定
#define PC_SHUTDOWN_PORT  7777
#define PC_SHUTDOWN_TOKEN "secure_pc_shutdown_token_12345" // 任意の長い推測困難な文字列
```

---

## 🚀 ビルド・書き込み手順

### 前提条件

- ESP-IDF v5.4.1 および `esp-matter` 環境がセットアップ済みであること
- `main/wifi_creds.h` が作成されていること

### ビルド & 書き込み

```bash
# 1. 環境変数のロード
source /home/kusa/esp/esp-idf/export.sh
source /home/kusa/esp/esp-matter/export.sh

# 2. ターゲット設定
idf.py set-target esp32c6

# 3. ビルド
idf.py build

# 4. 書き込み & モニター起動
idf.py -p /dev/ttyACM0 flash monitor
```

### 全消去（Erase）して再ペアリングする場合

```bash
# NVS / Matter ファブリック情報の完全消去
idf.py -p /dev/ttyACM0 erase-flash

# 再書き込み
idf.py -p /dev/ttyACM0 flash monitor
```

---

## 📱 Google Home ペアリング情報

- **方式**: On-Network Commissioning (Wi-Fi)
- **手動ペアリングコード**: `34970112332`
- **QR コード表示 URL**: [https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K90AFN00KA0648G00](https://project-chip.github.io/connectedhomeip/qrcode.html?data=MT%3AY.K90AFN00KA0648G00)
- **ファクトリーリセット**: 本体 GPIO 9 ボタンを 3 秒以上長押し

---

## 🔄 動作フロー

1. **電源 ON 操作 (Matter -> PC)**
   - Google Home 等から ON 操作を受信。
   - ESP32-C6 から Wi-Fi 経由で Wake-on-LAN (WOL) マジックパケット（`PC_MAC_ADDRESS` 宛）を送信。
   - PC が起動。

2. **電源 OFF 操作 (Matter -> PC)**
   - Google Home 等から OFF 操作を受信。
   - ESP32-C6 から Wi-Fi 経由で PC の `PC_SHUTDOWN_PORT` 宛てに TCP 接続。
   - `PC_SHUTDOWN_TOKEN`（トークン）を送信。
   - PC 側の常駐デモプログラム（デーモン）がトークンを認証し、`shutdown` を執行。

3. **PC 状態監視 & LCD 制御 (USB Status -> Matter / LCD)**
   - 2秒周期で USB Serial/JTAG 接続状態を監視。
   - 接続あり ➔ `PC: ON`（LCD 表示更新）
   - 接続なし ➔ `PC: OFF`（LCD 表示更新）
   - 状態乖離を検出した際、自動で Matter の OnOff 属性を同期更新。
   - **`PC: OFF` が 5 分間継続すると LCD バックライトを自動消灯（輝度0）。PC 起動で自動復帰。**

---

## 🐧 Ubuntu 側の常駐プログラム（デーモン）設定

ESP32 からのシャットダウン要求（TCP トークン）を待ち受け、安全に PC をシャットダウンするための常駐デーモンを systemd を使用して設定します。

### 1. デーモンスクリプトの確認・パーミッション設定

リポジトリ直下の `ubuntu_shutdown_daemon.py` を使用します。
スクリプト内の `PORT` と `TOKEN` が `wifi_creds.h` の設定と一致しているか確認してください。

> [!SECURITY]
> デーモンスクリプトにはシャットダウントークン（秘密情報）が直接書き込まれているため、root 以外のユーザーが読み書きできないように権限を制限することを推奨します。
> ```bash
> # 所有者を root に変更し、root のみ読み書き・実行可能に設定
> sudo chown root:root ubuntu_shutdown_daemon.py
> sudo chmod 700 ubuntu_shutdown_daemon.py
> ```

### 2. systemd サービスファイルの作成
`/etc/systemd/system/pc-shutdown-daemon.service` を作成し、デーモンをシステムサービスとして登録します。

まず、スクリプトをシステムの実行ファイル標準ディレクトリ `/usr/local/bin/` にコピーします。

```bash
# スクリプトを標準ディレクトリにコピー
sudo cp ubuntu_shutdown_daemon.py /usr/local/bin/
sudo chown root:root /usr/local/bin/ubuntu_shutdown_daemon.py
sudo chmod 700 /usr/local/bin/ubuntu_shutdown_daemon.py
```

次に、サービスファイルを作成します：

```bash
sudo nano /etc/systemd/system/pc-shutdown-daemon.service
```

以下の内容を貼り付けます：

```ini
[Unit]
Description=ESP32 SmartOn PC Shutdown Daemon
After=network.target

[Service]
Type=simple
# /usr/local/bin/ に配置したスクリプトを指定します。
ExecStart=/usr/bin/python3 /usr/local/bin/ubuntu_shutdown_daemon.py
Restart=always
RestartSec=5
User=root

[Install]
WantedBy=multi-user.target
```

### 3. サービスの有効化と起動

作成したサービスをリロードし、自動起動の設定および手動起動を行います。

```bash
# 1. systemdの再読み込み
sudo systemctl daemon-reload

# 2. 自動起動（OS起動時に自動で立ち上がるよう設定）
sudo systemctl enable pc-shutdown-daemon.service

# 3. デーモンの起動
sudo systemctl start pc-shutdown-daemon.service
```

### 4. 動作・ログ確認方法

正常に起動しているか、ログが出ているか確認します。

```bash
# 状態確認（Active: active (running) になっていれば正常）
sudo systemctl status pc-shutdown-daemon.service

# リアルタイムログ監視（ESP32から接続された際のログなどを確認できます）
sudo journalctl -u pc-shutdown-daemon.service -f
```