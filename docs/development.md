# 開発・メンテナンスガイド

このドキュメントでは、`hakoniwa-zenoh-topology-viewer` のソースを変更した後、
テストし、Hakoniwa Business PackのFoundationへ反映する方法を説明します。
講義を実施するだけの場合は、zenoh-tutorialの
`docs/06-viewer-setup.md`を参照してください。

## 1. 想定するworkspace

各リポジトリを同じworkspaceの直下へ配置します。

```text
workspace/
├── zenoh-tutorial/
├── hakoniwa-business-pack/
├── hakoniwa-zenoh-topology-viewer/
├── hakoniwa-pdu-endpoint/
├── hakoniwa-pdu-bridge-core/
├── hakoniwa-pdu-registry/
├── hakoniwa-pdu-javascript/
└── hakoniwa-pdu-python/
```

以下のFoundation向け手順は、Docker Composeの `node_a` 内で実行します。

## 2. 初回のみ必要な準備

Business PackのRecipeを使ってFoundationの設定を生成します。

```bash
cd /root/workspace/hakoniwa-business-pack

python3 tools/recipe.py doctor \
  --recipe recipes/examples/zenoh-tutorial-topology-viewer.yaml
python3 tools/recipe.py plan \
  --recipe recipes/examples/zenoh-tutorial-topology-viewer.yaml
python3 tools/recipe.py configure \
  --recipe recipes/examples/zenoh-tutorial-topology-viewer.yaml
```

Viewer用の生成済みビルド設定は次の場所に作られます。

```text
/root/workspace/hakoniwa-business-pack/work/foundation/build/hakoniwa-zenoh-topology-viewer.yaml
```

Foundationのインストール先は次の場所です。

```text
/root/workspace/hakoniwa-business-pack/work/foundation/install
```

## 3. Viewerを変更した後の標準手順

ホストの `workspace/zenoh-tutorial` から `node_a`へ入り、Viewerの
component-owned buildを直接実行します。

```bash
cd workspace/zenoh-tutorial
docker compose exec node_a bash

cd /root/workspace/hakoniwa-zenoh-topology-viewer

python3 tools/hako.py build \
  --config /root/workspace/hakoniwa-business-pack/work/foundation/build/hakoniwa-zenoh-topology-viewer.yaml

python3 tools/hako.py test \
  --config /root/workspace/hakoniwa-business-pack/work/foundation/build/hakoniwa-zenoh-topology-viewer.yaml

python3 tools/hako.py install \
  --config /root/workspace/hakoniwa-business-pack/work/foundation/build/hakoniwa-zenoh-topology-viewer.yaml \
  --install-dir /root/workspace/hakoniwa-business-pack/work/foundation/install

python3 tools/hako.py smoke \
  --config /root/workspace/hakoniwa-business-pack/work/foundation/build/hakoniwa-zenoh-topology-viewer.yaml \
  --install-dir /root/workspace/hakoniwa-business-pack/work/foundation/install
```

`recipe.py configure` はFoundation全体の依存関係を準備するコマンドです。
同じGit revisionの作業ツリーだけを変更した場合、既存receiptによってcomponentの
再構築が省略されることがあります。開発中の変更を確実に反映するには、上記の
`tools/hako.py build`、`test`、`install`を直接実行してください。

## 4. 変更箇所ごとの反映方法

| 変更箇所 | 必要な操作 |
| --- | --- |
| `docs/`、`README.md` | リビルド不要 |
| `web/src/`、`web/public/` | build、test、install、ブラウザの強制再読み込み |
| `collector/src/` | build、test、install、AggregatorまたはAgentの再起動 |
| Agentのheader／ABI | build、test、install、Viewer対応Cサンプルの再ビルド、Agentの再起動 |
| Launcher／inventory設定 | install後にLauncherを再起動 |
| `docker/Dockerfile`、Compose設定、OS package | Docker Composeを `--build` 付きで再作成 |

### Webだけを変更した場合

WebサーバーはFoundationへインストールされた静的ファイルを配信します。
標準手順の `install` まで実行した後、ブラウザを強制再読み込みします。

- macOS: `Command + Shift + R`
- Windows／Linux: `Ctrl + Shift + R`

通常、Launcherの再起動は不要です。古い画面が残る場合は、開発者ツールで
読み込まれたJavaScript asset名を確認してからLauncherを再起動します。

### CollectorまたはAgentを変更した場合

実行中のプロセスは、インストール前に読み込んだbinaryや共有ライブラリを使い
続けます。`install` 後にAggregatorと対象Agentを再起動してください。

AgentのAPI、header、ABIを変更した場合は、Viewer対応Cサンプルも再ビルドします。

```bash
cd /root/workspace/zenoh-tutorial/sample/c-sample
./build-viewer.bash
```

## 5. Launcherの再起動

Launcherを実行している端末で `Ctrl+C` を押し、次を再実行します。

```bash
cd /root/workspace/hakoniwa-business-pack

python3 tools/recipe.py launch \
  --recipe recipes/examples/zenoh-tutorial-topology-viewer.yaml
```

LauncherはBridge、Webサーバー、Aggregatorを起動します。`activate-only` modeでは
Hakoniwa Coreと `hako-cmd` は使用しません。

## 6. Docker環境から作り直す場合

Dockerfile、Compose設定、ビルド用OS packageを変更した場合に実施します。
通常のWeb／Collector変更だけで毎回行う必要はありません。

ホストの `workspace/zenoh-tutorial` で実行します。

WSL2／Ubuntu:

```bash
docker compose \
  -f docker-compose.yml \
  -f docker-compose.viewer.yml \
  up -d --build
```

Apple Silicon Mac:

```bash
docker compose \
  -f docker-compose.yml \
  -f docker-compose.viewer.yml \
  -f docker-compose.mac.yml \
  up -d --build
```

Foundationのvolumeが保持されている場合は、コンテナ再作成後も第3章の
component-owned buildが必要になることがあります。

## 7. 確認項目

反映後は次を確認します。

1. `tools/hako.py test` が成功する
2. `tools/hako.py smoke` が成功する
3. ブラウザが `connected` または想定した `partial` になる
4. ノード数、transport数、link数が想定どおりである
5. ノードまたはlinkを選択してもDetailsが次のsnapshotで消えない
6. Agent停止後に `stale` となり、再起動後に復旧する

表示内容の意味は[Zenoh Topology Viewerの見方](viewer-guide.md)を参照してください。
