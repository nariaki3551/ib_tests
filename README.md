# InfiniBand Multicast Performance Test

このディレクトリには、InfiniBandマルチキャストの性能テストプログラムが含まれています。複数ノードを跨いだマルチキャスト通信の性能を測定できます。

## 概要

このテストは以下の機能を提供します：

- InfiniBandデバイスの初期化
- UD（Unreliable Datagram）QPの作成と設定
- マルチキャストグループへの参加
- マルチキャストメッセージの送受信
- MPIを使用した複数ノードでのテスト実行
- **高性能なパフォーマンス測定機能**
- **詳細なログ出力とデータ整合性チェック**

## ファイル構成

- `ib_multicast_perf.c` - メインのパフォーマンステストプログラム
- `Makefile` - ビルド用Makefile
- `README.md` - このファイル

## 前提条件

### 必要なライブラリ

Ubuntu/Debian:
```bash
sudo apt-get install libibverbs-dev librdmacm-dev libopenmpi-dev
```

CentOS/RHEL:
```bash
sudo yum install libibverbs-devel librdmacm-devel openmpi-devel
```

### InfiniBandハードウェア

- InfiniBand HCA（Host Channel Adapter）がインストールされていること
- InfiniBandネットワークが設定されていること
- マルチキャストが有効になっていること

## ビルド

```bash
cd /app/ib_test

# プログラムをビルド
make

# または個別にビルド
make ib_multicast_perf
```

## 使用方法

### 1. 基本的なパフォーマンステスト

```bash
# ビルド
make

# 4ランクでパフォーマンステスト実行
mpirun -np 4 ./ib_multicast_perf

# またはMakefileターゲットを使用
make test
```

### 2. 複数ノードでのテスト

```bash
# パフォーマンステスト（複数ノード）
make run

# クイックパフォーマンステスト（単一ノード、4ランク）
make run-quick
```

### 3. カスタムオプションでのテスト

```bash
# デバイス名を指定
mpirun -np 4 ./ib_multicast_perf -d mlx5_0

# 複数ノードでの実行
mpirun -np 4 -hosts node1,node2,node3,node4 ./ib_multicast_perf

# パフォーマンスパラメータをカスタマイズ
mpirun -np 4 ./ib_multicast_perf -w 5 -i 50 -s 4 -l 2048 -u 1048576
```

## パフォーマンステスト機能

### 測定パラメータ

- **ウォームアップ回数**: 10回（`-w`オプションで変更可能）
- **測定回数**: 100回（`-i`オプションで変更可能）
- **メッセージサイズ**: 1KB ～ 1GB（`-l`、`-u`、`-s`オプションで変更可能）
- **測定メトリクス**:
  - 帯域幅（GB/s）
  - レイテンシ（マイクロ秒）

### 出力例

```
IB Multicast Performance Test
=============================
Warmup iterations: 10
Test iterations: 100
MPI ranks: 4

Size (bytes)        Bandwidth (GB/s) Latency (usec)
--------------------------------------------------------------------------------
1024 (1K)           0.022            45.23
2048 (2K)           0.037            52.67
4096 (4K)           0.056            68.91
8192 (8K)           0.076            101.45
16384 (16K)         0.092            167.23
32768 (32K)         0.103            298.67
65536 (64K)         0.108            567.89
131072 (128K)       0.111            1102.34
262144 (256K)       0.112            2189.67
524288 (512K)       0.113            4356.78
1048576 (1M)        0.114            8689.45
2097152 (2M)        0.115            17345.67
4194304 (4M)        0.116            34678.90
8388608 (8M)        0.117            69345.67
16777216 (16M)      0.118            138678.90
33554432 (32M)      0.119            277345.67
67108864 (64M)      0.120            554678.90
134217728 (128M)    0.121            1109345.67
268435456 (256M)    0.122            2218678.90
536870912 (512M)    0.123            4437345.67
1073741824 (1G)     0.124            8874678.90
--------------------------------------------------------------------------------
Performance test completed
```

## コマンドラインオプション

```
-d <device>      IB device name (default: first available)
-l <min_size>    Minimum message size in bytes (default: 1024)
-u <max_size>    Maximum message size in bytes (default: 1073741824)
-w <warmup>      Number of warmup iterations (default: 10)
-i <iterations>  Number of test iterations (default: 100)
-s <step>        Size step multiplier (default: 2)
-h               Show this help
```

## 環境変数

```
LOG_LEVEL        Set logging level (ERROR/0, INFO/1, DEBUG/2, default: INFO)
```

## ログレベル

- **ERROR (0)**: エラーメッセージのみ（赤色）
- **INFO (1)**: 情報メッセージ（緑色）
- **DEBUG (2)**: 詳細なデバッグ情報（通常色）

### ログ出力例

```
Rank 0: Starting IB multicast performance test: [main:1175]
Rank 0: Test sizes: 1024 to 1073741824 bytes (step: 2x) [main:1200]
Rank 0: Warmup iterations: 10, Test iterations: 100 [main:1201]
Rank 0: Warmup phase [run_performance_test:145]
Rank 0: Warmup iteration start [1/10] [run_performance_test:147]
Rank 1: Posted 1 receive WRs for 1024 bytes (chunk_size=984) [post_recv:320]
Rank 0: Sending 1024 bytes in 1 chunks (chunk_size=984) [post_send:350]
Rank 0: Batch send posted successfully (1 chunks; wr_ids 2-2) [post_send:420]
Rank 0: Waiting for completion... (expected WRs: 1) [wait_for_completion:450]
Rank 0: CQ entry found: wr_id=2, status=success [wait_for_completion:470]
Rank 0: Received 1 completion (wr_id=2) [wait_for_completion:480]
Rank 1: Waiting for completion... (expected WRs: 1) [wait_for_completion:450]
Rank 1: CQ entry found: wr_id=2, status=success [wait_for_completion:470]
Rank 1: Received 1 completion (wr_id=2) [wait_for_completion:480]
Rank 1: Verifying received data (size: 1024 bytes) [verify_received_data:520]
Rank 1: Data verification successful: all 1024 bytes are correct [verify_received_data:540]
```

## Makefileターゲット

```bash
# ビルド関連
make              # プログラムをビルド
make clean        # ビルドファイル削除

# テスト実行
make test         # パフォーマンステスト実行（4ランク）
make run          # マルチノードパフォーマンステスト
make run-quick    # クイックパフォーマンステスト（単一ノード）

# その他
make help         # ヘルプ表示
```

## 技術的な詳細

### マルチキャスト通信の仕組み

1. **ルートランク（Rank 0）**: フェイクマルチキャストアドレスでグループに参加し、実際のマルチキャスト情報を取得
2. **全ランク**: 実際のマルチキャストGIDでグループに参加
3. **送信**: ルートランクがマルチキャストメッセージを送信
4. **受信**: 他のランクがマルチキャストメッセージを受信

### パフォーマンス最適化

- **バッチ送信**: 大きなメッセージをMTUサイズに基づいてチャンク分割
- **複数WR**: 送信側と受信側で複数のWork Requestを使用
- **データ整合性チェック**: 受信したデータが正しい値を含むか検証
- **計測精度**: データコピーを計測の外側に移動

### バッファ管理

- **送信バッファ**: 1GBのメモリ領域
- **受信バッファ**: 同じ1GBのメモリ領域を共有
- **GRHバッファ**: 40バイトのGlobal Routing Header用バッファ
- **QP容量**: 最大16,384個のWork Request

## トラブルシューティング

### 1. InfiniBandデバイスが見つからない

```bash
# デバイス一覧を確認
ibv_devinfo

# デバイス名を指定してテスト
mpirun -np 4 ./ib_multicast_perf -d mlx5_0
```

### 2. マルチキャストグループに参加できない

- InfiniBandネットワークでマルチキャストが有効になっているか確認
- ファイアウォール設定を確認
- 異なるマルチキャストアドレスを試す

### 3. 権限エラー

```bash
# 必要に応じてsudoで実行
sudo mpirun -np 4 ./ib_multicast_perf
```

### 4. メモリ不足

```bash
# バッファサイズを小さくする（コード内でBUFFER_SIZEを変更）
# または、より少ないランク数でテスト
mpirun -np 2 ./ib_multicast_perf
```

### 5. タイムアウトエラー

```bash
# ログレベルをDEBUGに設定して詳細を確認
LOG_LEVEL=DEBUG mpirun -np 4 ./ib_multicast_perf
```

## 注意事項

- このプログラムはInfiniBand UD（Unreliable Datagram）を使用します
- マルチキャスト通信は信頼性が保証されません
- 大きなメッセージサイズではメモリ使用量が増加します
- パフォーマンス測定はネットワーク環境に大きく依存します 