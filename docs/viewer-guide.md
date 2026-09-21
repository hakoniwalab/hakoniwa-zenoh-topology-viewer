# Zenoh Topology Viewerの見方

このドキュメントでは、ブラウザに表示される図、ラベル、件数、
Detailsの意味を説明します。

## 1. 画面全体の見方

画面は大きく次の4つに分かれています。

```text
接続操作             WebSocket URI / Connect / Fit
集計値               nodes / transports / links / status
Topology graph       ノードとリンクの図
Details              全体または選択要素の詳細
```

- **Connect**: 指定したWebSocket Bridgeへ接続します
- **Fit**: 現在のグラフ全体が表示領域へ収まるようにします
- **Details**: 何も選択していない場合は全体情報、ノードまたはリンクを
  クリックした場合はその要素の情報を表示します
- グラフの背景をクリックすると、Detailsは全体情報へ戻ります

## 2. 丸や図形の意味

グラフ上の1つの図形は、`zid` で識別される1つの**Zenohノード**です。
Dockerコンテナ、物理マシン、Publisher、Subscriberを直接表すものでは
ありません。通常は1つのZenoh sessionが1つのZIDを持つため、同じ
コンテナ内でも複数sessionがあれば複数ノードとして見える場合があります。

図形と色はZenoh modeを表します。

| 表示 | `mode` | 意味 |
| --- | --- | --- |
| 青い丸 | `peer` | Peer modeのZenohノード |
| オレンジ色のひし形 | `router` | Router modeのZenohノード |
| 緑色の角丸四角形 | `client` | Client modeのZenohノード |
| 灰色の丸 | `unknown` | modeを観測結果から特定できなかったノード |

そのほかの表示状態は次のとおりです。

| 表示 | 意味 |
| --- | --- |
| 太い黒枠 | 単独Collector snapshotにおけるCollector自身 |
| 赤枠・半透明 | 対応するTopology Agentから一定時間更新がなく `stale` |
| 紫枠 | 現在選択してDetailsへ表示しているノード |

Aggregatorが生成するv2 snapshotでは複数Agentの情報を統合するため、
特定の1ノードをCollectorとして黒枠表示しない場合があります。

## 3. ノードのラベル

図形の下には、対応するTopology Agentを特定できる場合はAgent名を表示します。
Docker演習では `node_a:sub` や `node_b:pub` のように、実行ノードと
サンプル種別を組み合わせた名前になります。

- Agentが観測していない接続相手など、名前が分からないノードはZIDを表示します
- 12文字を超えるZIDは、先頭6文字と末尾4文字を
  `123456…cdef` のように省略表示します
- 完全なZIDは常にノードをクリックし、Detailsの `zid` で確認できます

Agent名は表示用の識別子であり、Zenoh sessionのZIDそのものではありません。

## 4. 線の意味

線はZenohノード間で観測されたlinkを表します。

線に方向はありません。PublisherからSubscriberへのデータ配送方向、
client／serverの関係、接続開始方向のいずれも表しません。

内部データには、次の観測関係が記録されています。

```text
source_zid（このlinkを報告した側） -- link -- remote_zid（接続相手）
```

Collectorは、自身が観測しているsessionを `source_zid`、Zenoh Connectivity
APIから得た接続相手を `remote_zid` として出力します。これは誰がlinkを観測
したかを保持するための値で、画面上の線に方向を付けるものではありません。

`src_endpoint` と `dst_endpoint` はDetailsで確認できますが、画面上の線に
方向を付けるための値ではありません。

同じ物理linkを両端のAgentが観測した場合、Aggregatorは両方の観測を1本へ
重複排除します。実際に観測したAgentは、線をクリックして
`raw.observed_by` で確認できます。

線の表示状態は次のとおりです。

| 表示 | 意味 |
| --- | --- |
| 灰色の実線 | 更新中のAgentから観測できているlink |
| 赤い破線・半透明 | このlinkを報告したすべてのAgentが `stale` |
| 紫色 | 現在選択してDetailsへ表示しているlink |

複数Agentのうち1つでも新しい観測を送っている場合、そのlinkはstale表示に
なりません。

## 5. 線のラベル

線の上に表示される `tcp`、`udp` などの文字列は、link endpointから取得した
通信プロトコルです。

```text
tcp
```

ポート番号は線上に表示しません。Zenoh Connectivity APIの `src_endpoint` と
`dst_endpoint` のどちらに待受ポートが現れるかは観測側によって変わり、もう
一方は一時的なクライアントポートになるためです。完全な両endpointは線を
クリックしてDetailsで確認してください。

これはZenoh key expressionやPublisher／Subscriber名ではありません。

プロトコルを取得できない場合、ラベルは空になります。

## 6. 上部の件数とstatus

### `nodes`

ZIDで重複を除いたZenohノード数です。`nodes` 配列だけでなく、transportや
linkの接続先として観測されたZIDも補完して数えます。

### `transports`

各AgentのZenoh sessionから観測したtransport数です。同じ接続を両端から
観測すると2件になります。

例えば3 Peer完全メッシュでは、3本の接続を両端から観測するため、通常は
`6 transports` になります。

### `links`

プロトコル、endpoint、接続ノードなどが同じ観測をAggregatorで重複排除した
link数です。3 Peer完全メッシュでは通常 `3 links` です。

同じ2ノード間に異なるendpointや複数のTCP接続がある場合は、線が複数表示
されることがあります。

### status

| 表示 | 意味 |
| --- | --- |
| `idle` | まだWebSocketへ接続していない |
| `live` | WebSocketとPDU readの準備は完了したが、最初のsnapshotは未受信 |
| `connected` | snapshotを受信し、すべての対象Agentが正常 |
| `partial` | 一部の対象からまだ取得できない、またはエラーがある |
| `partial: stale ...` | 表示されたAgentから一定時間更新がない |

## 7. 何も選択していないときのDetails

全体の集約状態を表示します。

```json
{
  "schema": "hakoniwa.zenoh.topology/v2",
  "collector_zid": "",
  "nodes": 3,
  "transports": 6,
  "links": 3,
  "status": "complete",
  "sources": []
}
```

| フィールド | 意味 |
| --- | --- |
| `schema` | Topology snapshotのschema。集約結果は通常 `hakoniwa.zenoh.topology/v2` |
| `collector_zid` | v1の単独Collector ZID。v2の集約結果では空になることがある |
| `nodes` | 表示対象の一意なZenohノード数 |
| `transports` | Agentごとのtransport観測数 |
| `links` | 重複排除後のlink数 |
| `status` | Aggregatorの状態。`complete` または `partial` |
| `sources` | Aggregatorが認識している各Topology Agentの取得状態 |

画面上部では、JSONの `complete` を利用者向けに `connected` と表示します。

### `sources` のフィールド

| フィールド | 意味 |
| --- | --- |
| `name` | 観測対象名。動的受付ではAgentの安定識別子（Docker演習では `node_a:sub` など） |
| `role` | 観測対象の役割。動的受付では `unknown` |
| `endpoint` | Agent snapshotを受信するHakoniwa PDU Endpoint設定。Zenoh link endpointではない |
| `zid` | そのAgentが観測しているZenoh sessionのZID |
| `status` | `ok`、`waiting`、`error`、`stale` のいずれか |
| `error` | 正常でない理由。正常時は省略されることがある |
| `last_received_at` | Aggregatorが最後にsnapshotを受信したUnix時刻。単位はミリ秒 |

`sources[].status` の意味は次のとおりです。

| 値 | 意味 |
| --- | --- |
| `ok` | 規定時間内にsnapshotを受信している |
| `waiting` | 起動後、まだ最初のsnapshotを受信していない |
| `error` | endpoint readやsnapshot検証でエラーになった |
| `stale` | 一度受信したが、`stale_after_ms` を超えて更新がない |

## 8. ノードをクリックしたときのDetails

```json
{
  "id": "a",
  "label": "a",
  "zid": "a",
  "mode": "peer",
  "collector": false,
  "stale": false
}
```

| フィールド | 意味 |
| --- | --- |
| `id` | Viewer内部で使用する要素ID。ノードではZIDと同じ |
| `label` | 図の下に表示するAgent名。名前が分からない場合は短縮済みZID |
| `name` | 対応するTopology Agent名。名前が分からない場合は空文字列 |
| `zid` | 完全なZenoh ID |
| `mode` | `peer`、`router`、`client`、`unknown` のいずれか |
| `collector` | 単独Collector自身なら `true` |
| `stale` | 対応するAgentの観測が古い場合は `true` |

選択中も毎秒のsnapshotは処理されます。選択したノードが存在する間、Detailsは
そのノードの最新値へ更新されます。ノードが消えた場合は全体Detailsへ戻ります。

## 9. リンクをクリックしたときのDetails

```json
{
  "id": "edge:link:a:b:tcp:...:1",
  "source": "a",
  "target": "b",
  "label": "tcp",
  "kind": "link",
  "stale": false,
  "raw": {}
}
```

| フィールド | 意味 |
| --- | --- |
| `id` | Viewerが安定した差分更新に使う内部ID |
| `source` | Viewer内部で線の一端として使う観測元ZID |
| `target` | Viewer内部で線のもう一端として使う接続相手ZID |
| `label` | 図に表示する通信プロトコル |
| `kind` | 通常は `link`。link情報がない場合のfallbackは `transport` |
| `stale` | この接続を報告したすべてのAgentが古い場合は `true` |
| `raw` | Collector／Aggregatorが出力した元のlinkまたはtransport情報 |

`id` は表示差分を追跡するための内部値です。Zenohが公開するIDではありません。

### `raw` がlinkの場合

| フィールド | 意味 |
| --- | --- |
| `source_zid` | このlink観測の起点となるZenoh ZID |
| `remote_zid` | 接続相手のZenoh ZID |
| `protocol` | `tcp`、`udp`などのlinkプロトコル |
| `src_endpoint` | 観測元側のendpoint |
| `dst_endpoint` | 接続相手側のendpoint |
| `group` | Zenoh Connectivity APIが返すlink group。空なら省略 |
| `mtu` | linkのMaximum Transmission Unit |
| `streamed` | byte stream型のlinkなら `true` |
| `interfaces` | linkに関連付けられたネットワークインターフェース |
| `auth_id` | 認証識別子。空なら省略 |
| `priority.min`／`priority.max` | 観測できた優先度の範囲 |
| `reliability` | `reliable` または `best_effort`。取得できない場合は省略 |
| `observed_by` | このlinkを観測したAgentのZID一覧 |

### `raw` がtransportの場合

link詳細を取得できないsnapshotでは、transportを線として表示します。

| フィールド | 意味 |
| --- | --- |
| `source_zid` | transportを観測したZenoh ZID |
| `remote_zid` | 接続相手のZenoh ZID |
| `remote_mode` | 接続相手のZenoh mode |
| `qos` | transportでQoSが有効なら `true` |
| `multicast` | multicast transportなら `true` |
| `shm` | shared memory transportなら `true`。未対応buildでは省略 |

## 10. timestampとschema

画面下部の `timestamp` は、snapshotを生成したUnix時刻のミリ秒値です。
人間向けの日時表記ではありません。

`schema` は受信したJSONの形式です。

- `hakoniwa.zenoh.topology/v1`: 1つのsessionを観測したsnapshot
- `hakoniwa.zenoh.topology/v2`: 複数AgentをAggregatorで統合したsnapshot

## 11. 図から判断できないこと

このViewerの図だけでは、次の情報は判断できません。

- どのノードがPublisherまたはSubscriberか
- どのkey expressionをpublish／subscribeしているか
- あるデータが実際にどちら向きへ配送されたか
- Router内部で選択されたデータ配送経路
- linkの帯域、遅延、メッセージ量

Viewerが表示するのは、各Zenoh sessionのConnectivity APIから観測した
**接続Topology**です。Pub/Sub関係やデータフローの可視化ではありません。
