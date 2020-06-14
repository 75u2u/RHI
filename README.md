# RHI ( Remort Haptic Interface )

RHIは，Wi-Fi環境下の遠隔地間で仮想的な触感の共有を実現するデバイスです．  

![パターン図](img/rhi_circuit.PNG "パターン図")

## 部品

- ESP-WROOM-32開発ボード
- 圧力センサ
- バイブレーションモータ
- バッテリー
- 抵抗10kΩ 

## ファイル
RemoteHapticInterface.ino：メインプログラム  
data/config.txt：wi-fiパスワードを自動で書き込むファイル  
img/rhi_circuit.PNG：パターン図

## 必要なライブラリ
WiFi.h  
FS.h  
SPIFFS.h  
TimeLib.h  
ESP32_WebGet.h  
MQTTClient.h  


## 関連資料
[取扱説明書](https://docs.google.com/presentation/d/1A6hZMrNEB62BZ_IO-AvbB-Tnsj0usCyL3eZcoHlCh6s/edit?usp=sharing)  
[技術仕様書](https://docs.google.com/document/d/1BT33lrkWnVqsaVDk59PR9j5sTTNF2xLjYFqtZrZAetg/edit?usp=sharing)  
