# Depo dökümü ve temizlik değerlendirmesi

Bu inceleme ana çalışma ağacındaki tüm dosyaları sınıflandırdı; mock bağımlılıkları, build hedefleri, test girdileri, runtime asset tüketicileri ve geçerli dokümanlar silme işleminden önce incelendi. İnceleme başlangıcı `2a47dca`; eski içerik `archive/pre-layout-cleanup-20261018` Git referansından kurtarılabilir. Beş tamamlanmamış worktree korunmuştur. Eşzamanlı mimari incelemenin `ARCHITECTURE.md` içine eklediği çalışma haritası/öneriler de korunur; bunlar uygulanmış özellik sayılmaz. Bu rapor bir GUI iyileştirmesi veya bütün özelliklerin tamamlandığı iddiası değildir.

## Asıl sorun neydi?

- Kaynak ağacı, eski tasarım otoritesi, tarihli durum anlatıları ve makine çıktıları birbirine karışmıştı. Eski mock hem ajanları yönlendiriyor hem de gerçek testlerin veri/ölçüm sağlayıcısı olarak kullanılıyordu.
- Native executable'lar root'ta, modül executable'ları test kaynaklarının yanında, çıktılar farklı dizinlerdeydi. Eski `clean` bunların tamamını kapsamıyordu.
- GitHub dil yüzdeleri ürünün runtime bağımlılıklarını anlatmıyordu. C99 `.inc` parçaları C++ sayılıyordu; tarayıcı testleri de JavaScript toplamına dahildi.
- Gerçek yapısal borç, yalnız dosya sayısı değil: büyük App çeviri birimi, ürün içinde native senaryolar, çok büyük tarayıcı test sürücüsü ve tekrarlanan derleme maliyeti.

Sürüm kontrolüne girebilen mevcut dosyaların toplamı yaklaşık **38,26 MB / 810 dosyadan 10,61 MB / 494 dosyaya** indi; yaklaşık %72 küçülme. Doküman ağacı yaklaşık 20 MB'tan 0,27 MB'a indi. Bunlar checkout kaynak boyutlarıdır; Git geçmişi, SDK, indirilen veri ve derleme çıktıları dahil değildir. Git geçmişi bilerek yeniden yazılmadı; bu yüzden `.git` boyutu aynı oranda küçülmez.

## Şimdi ne nerede?

| Konum | İçerik / karar |
| --- | --- |
| Root | `README.md`, `AGENTS.md`, `Makefile`, Git ayarları. Tek başlangıç ve yönlendirme; root executable yok. |
| `src/app/` | C App yaşam döngüsü, komutlar, işler, history ve seçim koordinasyonu. Büyük dosya borcu aşağıda. |
| `src/grid/`, `src/ui/` | C grid/geometri/input ve raylib widget/render/font kodu. |
| `src/data/`, `src/transforms/` | DuckDB, tipli sayfalar, worker/bridge; SQL/PIVOT/Marks/sayı/çizim semantiği. |
| `src/workbook/` | Seyrek hücreler, formatlar, nesneler, `.dxls` ve ortak metadata codec'i. |
| `src/platform/` | Portable sınır ve macOS AppKit adaptörü. Objective-C burada gereklidir. |
| `src/dev/` | Yalnız review derlemesine giren registry/capture/composer. Üretimde compile/link dışı. |
| `tests/` | Yazılmış C/Objective-C testleri; küçük CSV ve sabit regression girdileri. Executable veya koşu çıktısı değil. |
| `tools/` | Build/test süreçleri, native C veri üreticisi, offline ikon üretimi, gerçek browser otomasyonu ve dış feedback queue. Ayrıntılı dosya haritası [tools README](../tools/README.md). |
| `web/` | Aynı C UI'nin Wasm başlangıcı, browser API/Worker bağlantısı ve bunların testleri. Silinen mock ile ilgisi yok. |
| `assets/` | Gerçek runtime fontları/lisansları, canonical ikonlar, atlas ve bilinçli checked-in türevler. |
| `packaging/` | macOS/Linux/Windows paket metadatası ve küçük ikon türevleri. Platform çalıştırma kabulü anlamına gelmez. |
| `docs/` | Tek güncel durum/roadmap, gereksinimler, mimari, test rehberi, kullanıcı rehberi ve yaşayan ayrıntılı tasarımlar. |
| `.github/` | Gerçek Wasm build/test/Pages akışı; native hosted CI manuel/duraklatılmış. |
| `build/` | Tamamı ignored: binary, obje, generated fixture, sonuç, veri/SDK cache'i ve yerel app bundle. |

## Neler silindi, neler özellikle tutuldu?

**Silindi:** `design/` ve içindeki React/Vite mock, onun dependency/build/QA çıktıları; `docs/design/concepts`, `identity`, `typography` çalışmaları; eski font kopyaları; `docs/archive`, `docs/audit`, `docs/validation` tarihli payload ağaçları; duplicate root yönlendirmeleri. Mock exporter, mock screenshot araçları ve tamamlanmış tek seferlik asset importer/extractor kaldırıldı. Eski executable/dSYM ve `.DS_Store` artıkları temizlendi.

**Tutuldu:** persistence ve motor testleri, assertion'lar, sanitizer profilleri, runtime font/lisansları, ikon SVG/atlas/enum ve paket türevleri. Bunlar runtime veya yeniden üretilebilir test girdisi; sırf `generated` göründükleri için çöp değiller. Runtime'da font/ikon üretmek yerine küçük türevleri tutmak native build bağımlılıklarını azaltır.

Yedi authored CSV toplam **876 byte**; NULL/boş text/literal `NULL`, quoting ve sıralama gibi edge case'ler için gereklidir. Dört aktif UI ölçüm sözleşmesi yaklaşık **475 KB** olarak `tests/fixtures/ui/` altına taşındı. Mock kaynaklarını veya screenshot'larını açmazlar; kabul edilmiş native panel/scroll/SQL davranışının regression girdileridir. Kasıtlı UX değişirse bu sözleşmeler de bilinçli güncellenir.

`tests/shadow_corner_reference.h` yaklaşık 409 KB'lık test oracle'ıdır, kontrolsüz screenshot birikimi değildir. İsteğe bağlı corner diagnostic hâlâ kırmızı; istenen davranış kararı verilmeden testin girdisini silmek hata gizler. Bunun tamamlandığı söylenmiyor.

Eski içerik HEAD'de tutulmuyor. Gerektiğinde `git show archive/pre-layout-cleanup-20261018:<path>` ile geçmişten okunabilir; normal ajan bağlamına tekrar sokulmaz.

## Dillerin gerçek rolü

| Dil | Kullanım / değerlendirme |
| --- | --- |
| C99 | UI, input, motor/SQL tanımları, workbook/codec, native test assertion'ları ve artık deterministic veri üretimi. |
| C++ | Depoda `.cpp/.cc/.cxx/.hpp` kaynağı veya C++ ürün derlemesi yok. 30 `.inc` dosyası yaklaşık 1,33 MB C99 kodudur. GitHub Linguist bunları varsayılan C++ sayar; `.gitattributes` bunu C olarak düzeltiyor. DuckDB'nin kendi iç implementasyonu dış bağımlılıktır. |
| Objective-C | macOS native dosya panelleri, pencere/menü/kısayol/gesture ve entegrasyon testleri. Web teknolojisi veya ikinci UI değil. |
| JavaScript/MJS | Gerçek Wasm host'u ve browser input/DPR/clipboard/download/IndexedDB/official Worker sınırı; ayrıca Node/CDP testleri. Native binary'ye girmez. |
| Python | Süreç/timeout/kilit/sanitizer/fingerprint/fixture sıralaması, SDK/paketleme ve review queue/agent araçları. Native app bunlara ihtiyaç duymaz. |
| Makefile | Derleme/hedef/configuration grafiği. C ürün için normal geliştirme altyapısı. |

[Linguist dil tanımları](https://github.com/github-linguist/linguist/blob/main/lib/linguist/languages.yml) ve [override belgeleri](https://github.com/github-linguist/linguist/blob/main/docs/overrides.md) bu düzeltmenin dayanağıdır. JavaScript/Python'u dil istatistiğinden gizleyen bir kural eklenmedi. GitHub'daki yeni yüzdeler ancak değişiklik yayınlandıktan sonra hesaplanır.

`web/` kabaca 13 normal/review runtime modülü (~127 KB), 16 adapter unit test dosyası (~404 KB) ve 7 izole kabul sürücüsü (~111 KB) içerir. Yani çoğu testtir. `tools/test_web_app.mjs` ayrıca yaklaşık 606 KB'lık yoğun gerçek-browser sürücüsüdür; ürün UI'si değildir, ama bölünmeye ihtiyacı vardır.

## `make` ne üretir, temizlik ne yapar?

- `make`: native release app ve gereken objeler → `build/bin/duxcel`.
- `make duxcel-debug`: ayrı O1 ASan/UBSan app → `build/bin/duxcel-debug`.
- `make bundle`: `build/Duxcel.app`. Mevcut bundle Homebrew kitaplıklarına bağlı; bağımsız dağıtım henüz tamamlanmadı.
- Test hedefleri: `build/tests/test_*`, `build/tests/generated/`, `build/tests/results/`.
- Wasm hedefleri ayrı/explicit; `make` Node/browser/server başlatmaz. İkon kontrol damgası yoksa checked-in türev doğrulaması offline Python aracını çağırabilir.
- `make clean`: binary/obje/test/generated sonuçları ve Wasm uygulama paketlerini siler. `build/data/`, kurulu `build/web/toolchain` ve raylib cache'i ile `web/node_modules` tutulur.
- `make distclean`: bunlara ek olarak indirilen veri/SDK/raylib/Node cache'lerini de siler. Sonraki Wasm build bootstrap gerektirir; günlük kullanım için uygun değildir.

Kilit inode'ları silinmez. Test, Wasm build veya desktop sahibi varken temizlik reddedilir. Native runner'lar ortak checkout kilidini, web builder/raylib/SDK komutları ayrı sabit build kilidini kullanır. Kullanıcının açık uygulamasına dokunulmaz; manuel build/clean işlemleri çalışan kendi uygulamanızı siz kapattıktan sonra yapılmalıdır.

Yaklaşık 2 GB SDK,151 MB `web/node_modules` ve456 MiB public-data cache'i repo kaynağı değildir. Tekrar indirmenin maliyetini azaltırlar; açıkça `distclean` ile kaldırılabilirler. Başarılı geçici screenshot/log/fixture kümeleri source control'e girmez ve saklama politikasıyla budanır. Kullanıcı workbook'ları ve repo dışındaki review outbox bu temizliğin hedefi değildir.

## Screenshot ve agentic test akışı

Her build screenshot üretmez. İlgili native/browser senaryosu belirli state'lerde hidden framebuffer yakalar; assertion ve gerektiğinde görsel inceleme için kullanır. Otomatik bir “her commit'in önce/sonrası” sistemi yoktur. Anlamlı kıyas için aynı fixture, viewport, UI scale, sheet zoom ve state gerekir. Eski rastgele bir capture güncel kabulün kanıtı değildir.

Geçerli küçük sözleşmeler ve test kaynakları tutulur; bütün image/event/log kapsülleri commit edilmez. Bir hata araştırılırken gerekli birkaç kare ve failure kaydı kalabilir. Entegrasyon sonrası geçici başarılı koşu çıktıları silinebilir; concise sonuç, test komutu, source/binary hash ve süre yeterlidir. Ayrıntı için [test rehberi](TESTING.md).

## Ne kadarını C'ye taşıyabiliriz?

**İlk somut geçiş yapıldı:** mock JavaScript → CLI CSV → Parquet zinciri kaldırıldı. `tools/create_design_workbook.c` deterministic CSV'yi üretip DuckDB C API ile gerçek Parquet ve workbook yaratır, production PIVOT'u çalıştırır. Canonical310 ve büyük senaryo eski CSV ile byte-byte aynı; exact DECIMAL, quoted empty text ve SQL NULL kontrolleri normal/sanitizer derlemesinde geçer. Node, mock veya DuckDB CLI gerektirmez.

Sonraki mantıklı geçiş küçük C geliştirme CLI'sidir: süreç başlatma, deadline, log/exit kontrolü ve checkout/desktop kilidi. Python runner ancak timeout, crash, exit0 içindeki sanitizer hatası, eksik completion, binary değişimi, fixture sırası ve lock/refusal sözleşmeleri aynı şekilde geçerse kaldırılmalı. Ardından dosya/hash/queue işleri parça parça taşınabilir. Bunlar C ile yapılabilir; POSIX/macOS ve ileride Windows adaptörleri ile JSON/HTTP/SQLite/agent subprocess bakımını da üstlenmek gerekir. Mevcut güvenli çalışan queue'yu sırf dil yüzdesi için tek seferde yeniden yazmak isabetli değil.

Browser DOM/IndexedDB/WebGL/Worker sınırı JavaScript gerektirir. Bunları `EM_JS` string'lerine gömmek JavaScript'i ortadan kaldırmaz. Browser davranışına ait testleri tamamen native C testiyle ikame etmek de gerçek tarayıcı kabulünü kaybettirir. Ürün semantiğini C'de, platform köprüsünü dar tutmak doğru ayrımdır.

## Kalan yapısal işler ve öncelik

1. `src/app/app.c` yaklaşık 15.500 satır/1,22 MB. Native senaryolar aynı translation unit'te. Önce test-only sınırına kontrollü çıkarma, sonra gerektiğinde cohesive controller ayırma; büyük rewrite yok.
2. `src/dev/review_app.inc` yaklaşık 4.200 satır/441 KB; review-only olsa da tek dosya bakım maliyeti yüksek.
3. Modül testleri birçok ortak C dosyasını ayrı komutlarda tekrar derliyor. Per-file incremental cache ve açık bağımlılıklar test süresini azaltmak için daha doğrudan kazanç; assertion/wait silmek çözüm değil.
4. Tarayıcı CDP sürücüsünü workflow'lara bölmek, yaşam döngüsü/input helper'larını tekleştirmek gerekli. Python araçlarını rastgele alt klasörlere taşımak tek başına karmaşıklığı azaltmaz.
5. Browser workbook Open/Save, bütün candidate validation/migration/writer/recovery ve record-format kapsamı tamamlanmadı. Persistence internalleri/testleri roadmap'ten düşürülmedi. Tam native/browser input/parity ve standalone paketleme de açık.

## Bu değişikliğin doğrulaması

C fixture değişikliği `37b232c` ile başladı; layout/test-oracle değişiklikleri aynı entegrasyonun devamıdır. Ayrıntılı yerel koşular ignored sonuçlarda source/binary hash'leriyle ilişkilendirilir. Bu rapor her eski kabulü yeni source'a mal etmez.

- Runner: 36 sözleşme testi; temizliğin authored girdileri/cache'i koruması, aktif kilitte reddi ve CSV/JSON fingerprint değişimi dahil.
- Gerçek Make grafiği: 60 incremental/atomic kontrol, yaklaşık 23 saniye.
- C fixture: canonical/büyük, optimized ve ASan/UBSan; exact PIVOT/NULL/text/DECIMAL ve workbook çıktısı. Eski CSV byte eşitliği ayrıca doğrulandı.
- Grid/motion/cellreadout/workbook_codec: iki configuration; native active-cell 41 aşama dahil focused akış 39,3 saniye (native senaryo 20 saniye).
- Core ve workbook tam modülleri optimized/ASan/UBSan geçti: core 3,29/15,73 saniye; workbook 47,65/297,73 saniye. Ayrı focused format round-trip/rejection/original-preservation iki config toplam 17,4 saniye. Uzun sanitizer işi entegrasyon sınırında anlamlı; her editte tekrarlanmamalı.
- Gizli gerçek GPU/asset testi 110 kontrol, 3,34 saniye; bundle kök dizin `/` içinden altı aşamalı gerçek DuckDB smoke, 3,43 saniye. macOS bundle codesign doğrulandı.
- Node/browser adapter testleri 322, yaklaşık 3,23 saniye. Gerçek hidden DPR2 browser SQL gate 63,3 saniye ve son paketle panel/Steps rail gate 40,9 saniye; silinen mock olmadan çalıştı.
- Normal/review Wasm build ve production review-exclusion kontrolü geçti. Offline SDK sözleşmeleri 11, browser codec runner 24 ve Pages paketleme 6 test geçti.

Tam 1351-aşamalı native regression tekrar çalıştırıldığı iddia edilmiyor. Fiziksel trackpad/pinch/IME, Safari, Windows/Linux ve yeni bir görsel tasarım kabulü bu temizlik kapsamındaki testler değildir. İsteğe bağlı shadow corner diagnostic ve browser workbook Open/Save hâlâ açık işlerdir.
