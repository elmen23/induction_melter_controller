# ⚡ Induction Melter 1CH — ESP32 Web Controller

وحدة تحكم احترافية لـ ESP32 تتيح التحكم في **التردد** و **Duty Cycle** لمحوّل الحث (Induction Melter) أحادي القناة عبر **واجهة ويب حية** بالكامل.

> صُمّم خصيصاً للهواة والورش الصغيرة لصهر المعادن (ألمنيوم، نحاس، حديد، ذهب، فضة، رصاص…). كل الإعدادات تتم من المتصفح، بدون شاشة خارجية.

---

## ✨ الميزات

| الميزة                  | التفاصيل                                                                  |
|-------------------------|---------------------------------------------------------------------------|
| **التحكم في التردد**    | من 20 kHz إلى 200 kHz بخطوة 100 Hz                                        |
| **التحكم في Duty Cycle** | من 0% إلى 95% بخطوة 1%                                                    |
| **استطاعة خرج**         | حتى 3 kW (يمكن تعديلها في `config.h`)                                     |
| **PWM Hardware**        | LEDC peripheral — مؤقت مخصص، دقة 10-bit                                   |
| **وضع Half/Full bridge** | يمكن تفعيل أو تعطيل الخرج التكميلي (`setComplementary()`)                |
| **بداية ناعمة**         | Soft-start ramp تلقائي من 0% إلى الـ Duty المطلوب (800ms)                  |
| **PID مدمج**            | يضبط الـ Duty تلقائياً لمطابقة قدرة مستهدفة (V × I)                       |
| **حماية شاملة**         | ضد التيار الزائد، حرارة IGBT، حرارة المبرد، انقطاع المياه، عطل السواقة    |
| **E-STOP فيزيائي**      | زر مخصص يقطع الخرج فوراً (Hardware)                                       |
| **WiFi + AP fallback**  | يتصل بالشبكة المحفوظة، أو يفتح نقطة وصول للإعداد                         |
| **WebSocket**           | تحديث في الوقت الحقيقي (10 تحديثات/ث)                                     |
| **OTA**                 | رفع البرنامج الجديد من المتصفح                                            |
| **تسجيل بيانات**        | ملف CSV على SD Card (اختياري)                                             |
| **إعدادات مسبقة**       | معادن شائعة (ألمنيوم، نحاس، حديد، ذهب، فضة، رصاص، …)                    |
| **مصادقة HTTP Basic**   | حماية بكلمة مرور (admin / melter — غيّرها!)                               |
| **mDNS**                | `http://melter.local` بدل IP                                               |

---

## 🧰 العتاد المطلوب

| القطعة                | الكمية | ملاحظة                                                |
|-----------------------|--------|-------------------------------------------------------|
| ESP32 DevKit v1       | 1      | أي لوحة ESP32-WROOM-32                                |
| LED Driver Board      | 1      | Half/Full bridge (ZVS / Royer / IGBT)                 |
| ACS712-30A            | 1      | لقياس تيار الـ bus                                    |
| مقسم جهد 100k/10k     | 1      | لقياس جهد Vbus                                        |
| MAX6675 + Type-K      | 1      | لقياس حرارة البوتقة (اختياري)                          |
| NTC 10k B3950         | 2      | حرارة IGBT + حرارة المبرد                              |
| YF-S401 flow sensor   | 1      | استشعار تدفق المياه (Hall effect)                      |
| Buttons               | 2      | START (GPIO 2) + STOP/E-STOP (GPIO 0 — BOOT)           |
| Buzzer                | 1      | إنذارات صوتية                                         |
| Relay module          | 1      | فصل/وصل التيار الرئيسي                                |
| SD card module        | 1      | اختياري — لتسجيل البيانات                              |

> ⚠ **تنبيه أمان**: الجهد والتيار في دوائر الـ induction خطير. ضع كل العتاد في صندوق عازل، واستخدم قواطع مناسبة، ولا تلمس الملفات أثناء التشغيل.

---

## 🚀 طريقة البناء والتحميل

```bash
# 1) ثبّت PlatformIO
pip install -U platformio

# 2) استنسخ المشروع
git clone <repo> && cd induction_melter_controller

# 3) ارفع ملفات الواجهة إلى LittleFS
pio run --target uploadfs

# 4) ابنِ و ارفع البرنامج
pio run --target upload

# 5) راقب السيريال
pio device monitor
```

> عند الإقلاع الأول، ستظهر شبكة `Melter-XXXX` (مفتوحة). انضم إليها، وافتح `http://192.168.4.1` لإعداد WiFi.

---

## 🔌 التوصيل (Pinout)

> القيم الافتراضية في `include/config.h` — عدّلها حسب توصيلتك.

| الوظيفة              | GPIO | ملاحظة                              |
|----------------------|------|--------------------------------------|
| PWM Out A            | 25   | دخل driver الرئيسي                  |
| PWM Out B            | 26   | الطرف التكميلي (full-bridge)        |
| Driver Enable        | 27   | HIGH = يُسمح بالتشغيل               |
| Driver Fault         | 34   | دخل only — يفعّل عند LOW            |
| Current Sensor       | 35   | خرج ACS712                          |
| Vbus Sense           | 32   | مقسم جهد 100k/10k                    |
| MAX6675 CS           | 5    | SPI CS                               |
| MAX6675 SO / SD MISO | 18   | مشترك                               |
| MAX6675 SCK / SD SCK | 19   | مشترك                               |
| NTC IGBT             | 33   | مقسم جهد 10k                         |
| NTC Coolant          | 36   | input only                           |
| Flow Sensor          | 4    | مقاطعة، PULL-UP                      |
| START button         | 2    | على-board LED على معظم اللوحات       |
| STOP / E-STOP        | 0    | زر BOOT — لا تستعمله لتحميل البرنامج بعد التركيب! |
| Buzzer               | 13   |                                     |
| Relay Main           | 23   | كنتيكتور رئيسي                      |
| SD Card CS           | 14   |                                     |

> ⚠ الـ GPIO 0 هو زر BOOT على DevKit. استعماله كـ E-STOP يعني أنه بعد التركيب لن تستطيع رفع برنامج جديد عبر USB بسهولة. ضع jumper على GPIO 16/17 أو استعمل زر خارجي.

---

## 🌐 واجهة الويب

افتح `http://<IP>/` (أو `http://melter.local/` عبر mDNS). بيانات الدخول الافتراضية:

- **Username**: `admin`
- **Password**: `melter` — **غيّرها** في `config.h` (`MELTER_HTTP_USER` / `MELTER_HTTP_PASS`)

### التبويبات

1. **لوحة التحكم (Dashboard)** — قيم كبيرة للتردد/Duty/القدرة + أزرار Start/Stop/E-STOP + قراءات لحظية + رسم بياني حي
2. **التحكم (Control)** — منزلقات دقيقة + أزرار ±kHz / ±%
3. **المعادن (Presets)** — كبسات تحميل لإعدادات معدة مسبقاً
4. **PID** — تعديل Kp/Ki/Kd
5. **السجلات (Logs)** — حالة النظام + عداد الأخطاء
6. **الإعدادات (Settings)** — WiFi + OTA + إعادة التشغيل

### REST API

| المسار                  | الوظيفة                              |
|-------------------------|--------------------------------------|
| `GET /api/status`       | snapshot JSON بكل القراءات           |
| `GET /api/set/freq?v=HZ` | تعيين التردد                        |
| `GET /api/set/duty?v=PCT` | تعيين Duty                         |
| `GET /api/set/power?v=W` | تعيين القدرة المستهدفة              |
| `GET /api/preset?i=N`   | تحميل preset رقم N                   |
| `GET /api/presets`      | قائمة المعادن المعدة                |
| `GET /api/arm`          | تشغيل                                |
| `GET /api/disarm`       | إيقاف عادي                           |
| `GET /api/estop`        | إيقاف طارئ                           |
| `GET /api/clear`        | مسح الفاولت                          |
| `GET /api/pid/on?v=1`   | تفعيل/تعطيل PID                     |
| `GET /api/pid/tune?kp=&ki=&kd=` | تعديل المعاملات              |
| `GET /api/wifi/scan`    | فحص الشبكات                          |
| `GET /api/wifi/save?ssid=&pass=` | حفظ بيانات WiFi              |
| `POST /api/ota`         | رفع firmware (multipart)             |
| `GET /api/reboot`       | إعادة تشغيل                          |
| `GET /api/factory`      | مسح الإعدادات                        |

كل المسارات (ما عدا `/api/wifi/scan` و `/`) تتطلب HTTP Basic Auth.

### WebSocket

`ws://<IP>/ws` — يدفع JSON snapshot كل 100ms. يمكن إرسال أوامر:

```json
{ "cmd": "arm" }
{ "cmd": "disarm" }
{ "cmd": "estop" }
{ "cmd": "clearFault" }
```

---

## 🛡 فهرس الأخطاء

| الكود | الاسم                  | السبب                                  |
|-------|------------------------|------------------------------------------|
| 0     | OK                     | لا يوجد خطأ                             |
| 1     | Over current           | تيار الـ bus > 30A (افتراضي) لـ 200ms    |
| 2     | IGBT over-temp         | حرارة المشتت > 75°C                      |
| 3     | Coolant over-temp      | حرارة المبرد > 55°C                      |
| 4     | No coolant flow        | انقطاع أو ضعف تدفق المياه               |
| 5     | Driver fault           | دخل الـ fault في السواقة LOW             |
| 6     | Vbus over-voltage      | Vbus > 120V                              |
| 7     | Vbus under-voltage     | Vbus < 10V                               |
| 8     | Watchdog reset         | الفيد لم يُحدّث                           |
| 9     | Sensor read fail       | قراءة ADC/MAX6675 خاطئة                  |

لإعادة الضبط: من الواجهة انقر **إعادة ضبط الفاولت**، أو أرسل `GET /api/clear`.

---

## 📊 تسجيل البيانات (SD Card)

عند تركيب بطاقة SD، يُكتب ملف `/melter.csv` بصيغة:

```
epoch_ms,freq_hz,duty_pct,current_a,vbus_v,igbt_c,coolant_c,flow_lpm,state
123,50000,40,12.4,72.1,45,32,2.4,Run
...
```

يمكن استيراده إلى Excel/Pandas لتحليل الجلسات.

---

## 🧪 ضبط الـ PID

القيم الافتراضية (`Kp=2.0, Ki=0.5, Kd=0.1`) تعمل كنقطة بداية لـ 1.5kW. اضبطها من تبويب **PID** على الواجهة.

- **Kp عالي** = استجابة سريعة، لكن overshoot
- **Ki عالي** = يلغي الخطأ الثابت لكن يهتز
- **Kd عالي** = تخميد، لكن حساس للضوضاء

> **الطريقة السريعة**: ابدأ بـ `Kp=0`، ارفعه ببطء حتى يهتز، ثم اضربه في 0.5، أضف `Ki` صغير، اضبط `Kd` لتخميد.

---

## 📁 بنية المشروع

```
induction_melter_controller/
├── platformio.ini
├── min_spiffs.csv
├── include/
│   ├── config.h           ← كل الإعدادات
│   ├── logging.h
│   ├── power_controller.h ← PWM + freq + duty
│   ├── protection.h       ← فاولتات + snapshots
│   ├── user_input.h       ← START / STOP / E-STOP
│   ├── pid.h
│   ├── web_interface.h    ← WiFi + REST + WebSocket
│   └── logger.h           ← SD CSV
├── src/                   ← التنفيذ
├── data/                  ← ملفات الويب (LittleFS)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── docs/
    └── WIRING.md
```

---

## 🛠 خارطة طريق

- [ ] دعم multi-channel (2-4 قنوات)
- [ ] Ethernet (W5500) كبديل لـ WiFi
- [ ] MQTT integration
- [ ] Home Assistant discovery
- [ ] Modbus TCP server
- [ ] تطبيق Android (Kotlin)
- [ ] دعم التردد > 200kHz (يتطلب ESP32-S3 أو PCA9685)

---

## ⚖ الترخيص

MIT — استخدمه كيفما تشاء، لكنك وحدك مسؤول عن سلامتك وسلامة من حولك عند تشغيل محوّل حث.
