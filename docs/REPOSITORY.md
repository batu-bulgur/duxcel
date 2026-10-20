# Depo sınırları ve yöntem

Dosyalar uzantılarına veya üretildikleri session'a göre değil, sorumluluklarına göre yerleştirilir. Dil seçimi de kullandıkları API ve süreç sınırına dayanır. Bu kural yeni işler için geçerlidir; sırf dil yüzdesini değiştirmek için çalışan kod yeniden yazılmaz.

## Kaynak ve araçların sınırı

| Konum | Sorumluluk | Dil / çalışma sınırı |
| --- | --- | --- |
| `src/` | Native/ortak ürün modeli, UI, hesaplama tanımları, workbook ve platform adaptörleri | C99; macOS API sınırında Objective-C |
| `tests/` | Gerçek native ürün fonksiyonlarının davranış/bellek testleri | C99 ve macOS entegrasyonunda Objective-C |
| `tests/support/` | Ürün API'sine linklenen test yardımcıları | C99, ayrı test executable'ı |
| `tests/fixtures/` | Küçük, sabit ve açık tüketicisi olan girdiler/beklenen değerler | CSV, sürümlü JSON veya C pixel array'i |
| `tools/` | Süreç, dosya, ağ, build/paketleme, test çalıştırma ve dış feedback kuyruğu | Python standard library; AppKit araçlarında Objective-C |
| `tools/tests/` | Araçların hata, kilit, timeout, paketleme ve queue sözleşmeleri | Python unittest |
| `web/` | Aynı C uygulamasının browser API ve resmi DuckDB-Wasm Worker bağlantısı | İnce JS platform sınırı; ayrı JS spreadsheet UI yok |
| `web/tests/` | Browser adapter testleri, CDP sürücüleri ve test-only probe/host'lar | Node/JS; uygulama paketine girmez |
| `assets/`, `packaging/` | Runtime font/lisans/ikonları, bilinçli türevler ve platform metadatası | Build girdileri |
| `docs/` | Tek güncel plan/durum, gereksinimler, mimari ve rehberler | Eski session raporları değil |

Sentetik fixture üretimi `tools/create_workbook.py` içinde Python ile yapılır. Mevcut DuckDB CLI, CSV’yi Parquet’e çevirir ve `tests/fixtures/workbook/transactions.sql` içindeki sabit v12 test girdisini oluşturur. Genel workbook serializer’ının ikinci bir implementasyonu yoktur. `tests/test_fixture.c` üretim yapmaz; bu dosyaları gerçek ürün API’siyle açıp PIVOT, kesin DECIMAL, NULL/boş/literal metin ve save/reload davranışını sınar. Generator için C derlenmez; yeni Python paketi kurulmaz.

Test otomasyonu Python'dur; ürün fonksiyonlarına ilişkin native assertion'lar C'dir. Tarayıcı API sınırının ve gerçek Worker davranışının testleri JS'dir. Bunlar aynı işin rastgele farklı dillerde yazılmış kopyaları değildir. Yeni bir istisna somut API/platform/performance gerekçesi, açık arayüz ve ilgili test gerektirir. Büyük App/test-driver parçalarını ayırmak ayrı bakım işidir.

## Çıktı, cache ve indirilen veri

| Konum | İçerik | Temizlik |
| --- | --- | --- |
| `build/` | Executable/objeler, generated test DB/CSV/Parquet, screenshot/loglar, Wasm paketleri, macOS bundle | `make clean` tamamını kaldırır |
| `.cache/web/` | İndirilen SDK ve raylib dependency cache'i | `make distclean` kaldırır |
| `web/node_modules/` | Mevcut pinli browser bağımlılıkları | `make distclean` kaldırır |
| `data/downloads/` | Yellow Cab gibi tekrar kullanılabilen public referans verisi | İki clean hedefi de korur |
| `build/locks/` | Test/build işlemlerinin sahiplik kilitleri | İki clean hedefi de kaldırır |
| `__pycache__/` | Ignore edilen, yeniden üretilebilir Python bytecode'u | Kaynak/fixture/kayıt sayılmaz |

`clean` ve `distclean`, OS geçici dizinindeki checkout’a özgü kapıyı exclusive ve masaüstü kilidini alır. Build içindeki bir işlem kilidi kapıyı shared tuttuğundan temizlik çalışan işi silemez; temizlik sırasında yeni kilit inode’u açılamaz. Root’ta `.state/` tutulmaz. Eski `build/data` önce yeni veri konumuna korunarak taşınır; iki konum bağımsız doluysa üzerine yazmak yerine temizlik reddedilir. Normal clean eski SDK/raylib cache'ini de yeni cache konumuna taşır. SDK'nın absolute configuration'ı taşındıysa ilk web kullanımında offline bootstrap gerekir. Kullanıcı workbook'ları ve repo dışındaki review outbox bu işlemlerin hedefi değildir.

İndirilen veri explicit `python3 tools/download_test_data.py` ile alınır. Hiçbir test/build otomatik download başlatmaz. `--roundtrip` istenirse mevcut DuckDB CLI ile türetilen CSV/Parquet `build/tests/generated/public-data/` içine yazılır; orijinaller veri klasöründe kalır. Kalıcı girdiyi generated çıktı gibi silmeyiz.

## Fixture mı, eski koşu kalıntısı mı?

Bir fixture'ın ismi, şeması ve gerçek tüketicisi olmalıdır. Yedi küçük CSV NULL/text/quoting/sort/chart durumlarını sınar. Üç UI JSON'u yalnız kabul edilmiş panel/geometri/scroll durumlarını ve beklenen değerleri tutar; [fixture haritası](../tests/fixtures/ui/README.md) her birini açıklar. İki rendering header'ı native pixel testlerinin girdisidir.

Eski kaynak/prototype manifestleri, komutlar, süre/pass raporları ve kullanılmayan capture state'leri fixture değildir. Bu tur bunlar JSON'lardan çıkarıldı; üç eski session/provenance JSON'u ve eski screenshot dizinine bağımlı verifier kaldırıldı. Kabul edilmiş geometri/scroll/renk projeksiyonları önce/sonra aynı kaldı ve bağımsız fingerprint testiyle korunuyor. Araç testleri ile browser sürücüleri kendi test dizinlerine taşındı; Fixture üretimi Python’da, ürün kabul assertion’ları `tests/test_fixture.c` içindedir.

Runtime font/ikon lisansları ve bilinçli checked-in türevler korunur. Eski executable mock/görsel denemeler source'a geri getirilmez. Tarihçe Git'te bulunur; ayrıca filesystem arşivi, bundle, tar veya patch yedeği tutulmaz. HEAD içinde audit/archive/screenshot payload ağacı biriktirilmez. Eski branch uçları ve worktree değişiklikleri Git'teki `archive/streamline-20261020` etiketi altında korunur; yarım işlerin güncel durumu roadmap’te izlenir.

## Kontrol ve kalan işler

`tools/tests/test_layout.py` bu rol sınırlarını denetler. Araç/loader sözleşmeleri, native C fixture/sanitizer ve odaklı native testler ilgili değişiklikte çalıştırılır; tam native/browser/OS kabulü ayrı kapsamdır. Güncel capability sınırları [STATUS.md](STATUS.md), kalan ürün/persistence işleri [ROADMAP.md](ROADMAP.md) içindedir.

Yaklaşık15.500 satırlık App içinden native senaryoların test-only sınıra çıkarılması, modül testlerinin incremental derlemesi ve büyük browser sürücüsünün workflow'lara bölünmesi hâlâ gereklidir. Bu yerleşim düzeltmesi bunların tamamlandığını, autosave/formül veya browser workbook Open/Save geldiğini iddia etmez.
