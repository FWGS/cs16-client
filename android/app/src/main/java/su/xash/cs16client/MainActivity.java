package su.xash.cs16client;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.CheckBox;
import android.widget.SeekBar;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * CS 1.6 boost-top Mobile - boshlang'ich ekran.
 * boost-top.com monitoringidan TOP / BOOST / VIP / Sevimlilar serverlarini ko'rsatadi,
 * o'yin fayllarini yuklab beradi va Xash3D FWGS dvijokini kerakli server bilan ishga tushiradi.
 */
public class MainActivity extends Activity {

    // ---------------- sozlamalar (saytdagi mobile.json ularni almashtira oladi) ----------------
    private static final String SERVERS_URL = "http://mmb.boost-top.com/servers.php";
    private static final String CONFIG_URL = "http://mmb.boost-top.com/mobile.json";
    // Dvijok bu manzilga "/v1/servers/cstrike" qo'shib so'raydi
    private static final String STATS_URL = "http://mmb.boost-top.com/stats.php";
    private static final String MASTER_URL = "http://mmb.boost-top.com/ms.php?q=";
    private String dataUrl = "http://mmb.boost-top.com/mobile/cs16_data.zip";
    private String engineUrl = "https://github.com/FWGS/xash3d-fwgs/releases/tag/continuous";
    private String siteUrl = "https://boost-top.com";
    private int dataSizeMb = 250;
    // avtomatik yangilanish: saytdagi mobile.json -> apk_build / apk_url
    private volatile int apkBuild = 0;
    private volatile String apkUrl = "";
    // yangiliklar: saytdagi mobile.json -> "news"
    private volatile JSONArray news = null;

    // ---------------- ranglar ----------------
    private static final int C_BG = 0xFF0A1426;
    private static final int C_CARD = 0xFF13213A;
    private static final int C_LINE = 0xFF22385C;
    private static final int C_TEXT = 0xFFEAF2FF;
    private static final int C_MUTED = 0xFF9FB6D6;
    private static final int C_ACCENT = 0xFF1E8CFF;
    private static final int C_GOLD = 0xFFFFB21E;
    private static final int C_MAP = 0xFFF39A1E;
    private static final int C_PL = 0xFF8E5CF0;
    private static final int C_OFF = 0xFF5A6476;

    private LinearLayout content;
    private TextView status;
    private final Map<String, Bitmap> flags = new HashMap<String, Bitmap>();
    private String lang = "uz";
    private volatile boolean downloading = false;
    private ProgressBar dlBar;
    private TextView dlText;
    private JSONObject lastData;
    // ping: telefondan har bir serverga UDP so'rov (A2S_INFO) javob vaqti, ms
    private final Map<String, Integer> pings = new java.util.concurrent.ConcurrentHashMap<String, Integer>();
    private final Map<String, TextView> pingViews = new HashMap<String, TextView>();
    private volatile boolean pinging = false;

    // ---------------- matnlar ----------------
    private String t(String key) {
        String[][] tr = {
                // key, uz, ru, en
                {"title", "CS 1.6 boost-top", "CS 1.6 boost-top", "CS 1.6 boost-top"},
                {"subtitle", "Eng yaxshi serverlar", "Лучшие серверы", "Best servers"},
                {"join", "ULANISH", "ИГРАТЬ", "PLAY"},
                {"play", "O'YINNI OCHISH", "ОТКРЫТЬ ИГРУ", "OPEN GAME"},
                {"refresh", "Yangilash", "Обновить", "Refresh"},
                {"loading", "Serverlar yuklanmoqda...", "Загрузка серверов...", "Loading servers..."},
                {"neterr", "Internetga ulanib bo'lmadi. Yangilash tugmasini bosing.", "Нет соединения. Нажмите «Обновить».", "No connection. Tap Refresh."},
                {"empty", "Hozircha serverlar yo'q", "Пока нет серверов", "No servers yet"},
                {"offline", "OFFLINE", "OFFLINE", "OFFLINE"},
                {"setup", "O'yinni sozlash (bir marta)", "Настройка игры (один раз)", "Game setup (one time)"},
                {"s1", "1. Xash3D dvijokini o'rnating", "1. Установите движок Xash3D", "1. Install the Xash3D engine"},
                {"s1b", "YUKLAB OLISH", "СКАЧАТЬ", "DOWNLOAD"},
                {"s2", "2. Dvijokni bir marta oching va fayllarga ruxsat bering, keyin shu yerga qayting", "2. Откройте движок один раз и разрешите доступ к файлам, затем вернитесь сюда", "2. Open the engine once, allow file access, then come back"},
                {"s2b", "DVIJOKNI OCHISH", "ОТКРЫТЬ ДВИЖОК", "OPEN ENGINE"},
                {"s3", "O'yin fayllarini yuklab oling (bir marta)", "Скачайте файлы игры (один раз)", "Download game files (one time)"},
                {"s3b", "YUKLASH", "СКАЧАТЬ", "DOWNLOAD"},
                {"done", "✓ Tayyor", "✓ Готово", "✓ Done"},
                {"perm", "Fayllarni saqlash uchun ruxsat bering", "Разрешите доступ к файлам для сохранения", "Allow file access to save files"},
                {"dlwarn", "Taxminan %d MB yuklanadi. Wi-Fi tavsiya etiladi. Davom etamizmi?", "Будет загружено около %d МБ. Рекомендуется Wi-Fi. Продолжить?", "About %d MB will be downloaded. Wi-Fi recommended. Continue?"},
                {"yes", "Ha", "Да", "Yes"},
                {"no", "Yo'q", "Нет", "No"},
                {"dling", "Yuklanmoqda: ", "Загрузка: ", "Downloading: "},
                {"dlok", "O'yin fayllari tayyor!", "Файлы игры готовы!", "Game files are ready!"},
                {"dlerr", "Yuklashda xato: ", "Ошибка загрузки: ", "Download error: "},
                {"needsetup", "Avval o'yinni sozlang (yuqoridagi qadamlar)", "Сначала настройте игру (шаги выше)", "Set up the game first (steps above)"},
                {"upd", "Yangi versiya chiqdi!", "Вышла новая версия!", "New version available!"},
                {"updb", "YANGILASH", "ОБНОВИТЬ", "UPDATE"},
                {"updhint", "APK yuklanadi - keyin uni ochib o'rnating. O'yin fayllari saqlanib qoladi.", "APK скачается - откройте его и установите. Файлы игры сохранятся.", "The APK will download - open it to install. Game files are kept."},
                {"nick", "O'yindagi nik", "Ник в игре", "In-game nickname"},
                {"gfx", "Grafika", "Графика", "Graphics"},
                {"gfx_high", "Yuqori", "Высокая", "High"},
                {"gfx_mid", "O'rta", "Средняя", "Medium"},
                {"gfx_low", "Past (tez)", "Низкая (быстро)", "Low (fast)"},
                {"fps", "FPS ni ko'rsatish", "Показывать FPS", "Show FPS"},
                {"favs", "⭐ Mening serverlarim", "⭐ Мои серверы", "⭐ My servers"},
                {"players", "O'yinchilar", "Игроки", "Players"},
                {"pl_nick", "Nik", "Ник", "Name"},
                {"pl_score", "Frag", "Фраги", "Score"},
                {"pl_time", "Vaqt", "Время", "Time"},
                {"pl_none", "Hozir serverda hech kim yo'q", "Сейчас на сервере никого нет", "Nobody is playing right now"},
                {"pl_err", "Server javob bermadi", "Сервер не ответил", "Server did not respond"},
                {"close", "Yopish", "Закрыть", "Close"},
                {"updperm", "Ruxsat bering: \"Shu manbadan o'rnatish\" ni yoqing, keyin qaytib YANGILASH ni bosing", "Разрешите установку из этого источника, затем вернитесь и нажмите ОБНОВИТЬ", "Allow installs from this source, then come back and tap UPDATE"},
                {"updinst", "O'rnatilmoqda... Ilova yopilsa - qayta oching, yangi versiya tayyor bo'ladi.", "Устанавливается... Если приложение закроется - откройте его снова, новая версия будет готова.", "Installing... If the app closes, open it again - the new version will be ready."},
                {"updwarn", "Yangi versiya yuklanadi. Keyin telefonning O'RNATISH oynasi chiqadi: \"Yangilash/O'rnatish\" ni, tugagach \"Ochish\" ni bosing. O'yin fayllari saqlanib qoladi.", "Новая версия скачается. Затем откроется окно УСТАНОВКИ телефона: нажмите \"Обновить/Установить\", а после - \"Открыть\". Файлы игры сохранятся.", "The new version will download. Then your phone's INSTALL screen opens: tap \"Update/Install\", then \"Open\". Game files are kept."},
                {"updok", "Davom etish", "Продолжить", "Continue"},
                {"upderr", "Yangilab bo'lmadi", "Не удалось обновить", "Update failed"},
                {"updretry", "Qayta urinish", "Повторить", "Retry"},
                {"updbrowser", "Brauzer orqali", "Через браузер", "Via browser"},
                {"settings", "Sozlamalar", "Настройки", "Settings"},
                {"lang", "Til", "Язык", "Language"},
                {"sens", "Sichqoncha / ekran sezgirligi", "Чувствительность", "Sensitivity"},
                {"xsize", "Pritsel o'lchami", "Размер прицела", "Crosshair size"},
                {"xcolor", "Pritsel rangi", "Цвет прицела", "Crosshair color"},
                {"xthick", "Pritsel qalinligi", "Толщина прицела", "Crosshair thickness"},
                {"xt0", "Klassik", "Классика", "Classic"},
                {"xt1", "Ingichka", "Тонкий", "Thin"},
                {"xt2", "Qalin", "Толстый", "Thick"},
                {"xt3", "Juda qalin", "Очень толстый", "Extra thick"},
                {"xdot", "Markazda nuqta", "Точка в центре", "Center dot"},
                {"ctrl", "Boshqaruv", "Управление", "Controls"},
                {"gyro", "Giroskop bilan nishon olish (PRO)", "Прицеливание гироскопом (PRO)", "Gyro aiming (PRO)"},
                {"gyrosens", "Giroskop sezgirligi", "Чувствительность гироскопа", "Gyro sensitivity"},
                {"ctrl_pro", "PRO (2 ta o'q tugmasi)", "PRO (2 кнопки огня)", "PRO (2 fire buttons)"},
                {"ctrl_std", "Standart", "Стандарт", "Standard"},
                {"auto", "Avto", "Авто", "Auto"},
                {"small", "Kichik", "Малый", "Small"},
                {"medium", "O'rta", "Средний", "Medium"},
                {"large", "Katta", "Большой", "Large"},
                {"save", "Saqlash", "Сохранить", "Save"},
                {"cancel", "Bekor qilish", "Отмена", "Cancel"},
                {"saved", "Saqlandi. O'yinda qo'llanadi.", "Сохранено. Применится в игре.", "Saved. Applied in game."},
                {"ad", "Serveringizni shu yerga chiqarish: boost-top.com", "Разместить свой сервер здесь: boost-top.com", "Promote your server here: boost-top.com"},
        };
        int col = "ru".equals(lang) ? 2 : ("en".equals(lang) ? 3 : 1);
        for (String[] row : tr) if (row[0].equals(key)) return row[col];
        return key;
    }

    private int dp(float v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics());
    }

    private GradientDrawable round(int color, float radiusDp, int strokeColor) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(dp(radiusDp));
        if (strokeColor != 0) d.setStroke(dp(1), strokeColor);
        return d;
    }

    private TextView text(String s, float sp, int color, boolean bold) {
        TextView tv = new TextView(this);
        tv.setText(s);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        tv.setTextColor(color);
        if (bold) tv.setTypeface(Typeface.DEFAULT_BOLD);
        return tv;
    }

    private Button button(String s, int color) {
        Button b = new Button(this);
        b.setText(s);
        b.setTextColor(Color.WHITE);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setAllCaps(false);
        b.setBackground(round(color, 8, 0));
        b.setPadding(dp(14), dp(6), dp(14), dp(6));
        b.setMinHeight(dp(38));
        b.setMinimumHeight(dp(38));
        return b;
    }

    // ---------------- hayotiy sikl ----------------
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        String l = Locale.getDefault().getLanguage();
        String defLang = "ru".equals(l) || "uk".equals(l) || "be".equals(l) || "kk".equals(l) ? "ru" : ("en".equals(l) ? "en" : "uz");
        lang = prefs().getString("lang", defLang);

        if (Build.VERSION.SDK_INT >= 21) {
            getWindow().setStatusBarColor(C_BG);
            getWindow().setNavigationBarColor(C_BG);
        }

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(C_BG);

        // sarlavha
        LinearLayout head = new LinearLayout(this);
        head.setOrientation(LinearLayout.HORIZONTAL);
        head.setGravity(Gravity.CENTER_VERTICAL);
        head.setPadding(dp(16), dp(14), dp(12), dp(10));
        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.addView(text(t("title"), 20, C_TEXT, true));
        titles.addView(text(t("subtitle"), 13, C_GOLD, true));
        head.addView(titles, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        Button refresh = button(t("refresh"), C_LINE);
        refresh.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                loadServers();
            }
        });
        Button gear = button("⚙", C_LINE);
        gear.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        gear.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showSettings();
            }
        });
        LinearLayout.LayoutParams glp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        glp.setMargins(0, 0, dp(8), 0);
        head.addView(gear, glp);
        head.addView(refresh);
        root.addView(head);

        ScrollView scroll = new ScrollView(this);
        content = new LinearLayout(this);
        content.setOrientation(LinearLayout.VERTICAL);
        content.setPadding(dp(12), 0, dp(12), dp(12));
        scroll.addView(content);
        root.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        // pastki panel
        LinearLayout foot = new LinearLayout(this);
        foot.setOrientation(LinearLayout.VERTICAL);
        foot.setPadding(dp(12), dp(8), dp(12), dp(12));
        foot.setBackgroundColor(0xFF081020);
        Button play = button(t("play"), C_ACCENT);
        play.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                launchGame(null);
            }
        });
        foot.addView(play, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(46)));
        TextView ad = text(t("ad"), 12, C_MUTED, false);
        ad.setGravity(Gravity.CENTER);
        ad.setPadding(0, dp(8), 0, 0);
        ad.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openUrl(siteUrl);
            }
        });
        foot.addView(ad);
        root.addView(foot);

        setContentView(root);
        loadConfig();
        registerInstallReceiver();
        sendStat("open", null);
    }

    @Override
    protected void onResume() {
        super.onResume();
        render();
        loadServers();
    }

    // ---------------- dvijok va o'yin fayllari ----------------
    // Dvijok (Xash3D FWGS) shu ilovaning ichida - alohida o'rnatish shart emas.
    // O'yin fayllari ilovaning o'z papkasida saqlanadi - hech qanday ruxsat kerak emas.
    private File baseDir() {
        File ext = getExternalFilesDir(null);
        if (ext == null) ext = getFilesDir();
        return new File(ext, "xash");
    }

    private boolean hasData() {
        File b = baseDir();
        return new File(b, "valve/liblist.gam").exists() && new File(b, "cstrike/liblist.gam").exists();
    }

    private boolean setupDone() {
        return hasData();
    }

    /** Saytdagi ro'yxatdan qat'i nazar, o'yin ichidagi "Favorites" ning boshida doim turadigan serverlar. */
    private static final String[] FAV_FIXED = { "212.47.58.89:27015", "212.47.58.40:27015" };

    /** O'yin ichidagi "Favorites" bo'limiga saytdagi serverlarni yozadi (cstrike/favorite_servers.lst). */
    private void writeFavorites() {
        try {
            JSONArray sections = lastData == null ? null : lastData.optJSONArray("sections");
            if (sections == null) sections = new JSONArray();
            StringBuilder sb = new StringBuilder();
            java.util.HashSet<String> seen = new java.util.HashSet<String>();
            // har doim "Favorites" da turadigan serverlar (egasining so'rovi) - saytdan ro'yxat kelmasa ham
            for (String ip : FAV_FIXED) if (seen.add(ip)) sb.append(ip).append(" gs\n");
            for (int i = 0; i < sections.length(); i++) {
                JSONObject s = sections.optJSONObject(i);
                JSONArray list = s == null ? null : s.optJSONArray("servers");
                if (list == null) continue;
                for (int k = 0; k < list.length(); k++) {
                    JSONObject sv = list.optJSONObject(k);
                    String ip = sv == null ? "" : sv.optString("ip", "");
                    if (ip.length() > 0 && seen.add(ip)) sb.append(ip).append(" gs\n");
                }
            }
            File dir = new File(baseDir(), "cstrike");
            if (!dir.exists()) return;
            // master-server manzili qo'shtirnoq ichida bo'lishi shart ("//" izoh deb o'qilmasin)
            FileOutputStream ms = new FileOutputStream(new File(dir, "boosttop_ms.cfg"));
            ms.write(("addmasterstatic \"" + MASTER_URL + "\"\n").getBytes("UTF-8"));
            ms.close();
            // O'yin menyusidagi "Случайный сервер" tugmasi uchun: ro'yxatimizdan tasodifiy onlayn server
            java.util.List<String> online = new java.util.ArrayList<String>();
            for (int i = 0; i < sections.length(); i++) {
                JSONObject s = sections.optJSONObject(i);
                JSONArray list = s == null ? null : s.optJSONArray("servers");
                if (list != null) for (int k = 0; k < list.length(); k++) {
                    JSONObject sv = list.optJSONObject(k);
                    if (sv != null && sv.optBoolean("online", false)) online.add(sv.optString("ip"));
                }
            }
            String rnd = online.isEmpty() ? "" : online.get(new java.util.Random().nextInt(online.size()));
            FileOutputStream rf = new FileOutputStream(new File(dir, "boosttop_random.cfg"));
            rf.write((rnd.length() > 0 ? "connect " + rnd + "\n" : "echo no servers\n").getBytes("UTF-8"));
            rf.close();
            // Menyu tugmalari kompyuterdagidek matn ko'rinishida (tanlangan tilda)
            File lib = new File(dir, "liblist.gam");
            if (lib.exists()) {
                String txt = new String(readAll(lib), "UTF-8");
                if (!txt.contains("render_picbutton_text")) {
                    FileOutputStream lo = new FileOutputStream(lib, true);
                    lo.write("\nrender_picbutton_text \"1\"\n".getBytes("UTF-8"));
                    lo.close();
                }
            }
            if (sb.length() == 0) return;
            FileOutputStream fo = new FileOutputStream(new File(dir, "favorite_servers.lst"));
            fo.write(sb.toString().getBytes("UTF-8"));
            fo.close();
        } catch (Exception ignored) {
        }
    }

    private static byte[] readAll(File f) throws java.io.IOException {
        java.io.FileInputStream in = new java.io.FileInputStream(f);
        try {
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            return bo.toByteArray();
        } finally {
            in.close();
        }
    }

    private void launchGame(String ip) {
        if (!hasData()) {
            toastLike(t("needsetup"));
            return;
        }
        writeFavorites();
        writeSettings();
        // O'yin ichidagi server ro'yxatida faqat boost-top.com serverlari chiqsin:
        // standart (begona) master-serverlarni o'chirib, o'zimiznikini qo'shamiz.
        String argv = "-log -dll @yapb +clearmasters +exec boosttop_ms.cfg +exec boosttop_settings.cfg" + touchPresetArg();
        if (ip != null) {
            argv += " +connect " + ip;
            sendStat("connect", ip);
        }
        try {
            startActivity(new Intent().setComponent(new ComponentName(getPackageName(), "su.xash.engine.XashActivity"))
                    .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK)
                    .putExtra("gamedir", "cstrike")
                    .putExtra("gamelibdir", getApplicationInfo().nativeLibraryDir)
                    .putExtra("basedir", baseDir().getAbsolutePath())
                    .putExtra("argv", argv)
                    .putExtra("package", getPackageName()));
        } catch (Exception e) {
            toastLike(String.valueOf(e.getMessage()));
        }
    }

    // ---------------- PRO boshqaruv (tugmalar joylashuvi) ----------------
    private static final String TOUCH_PRO = "// boost-top PRO boshqaruv: chap qo'lda ham O'Q tugmasi (o'ng barmoq nishonda qoladi)\ntouch_removeall\ntouch_pitch \"50\"\ntouch_yaw \"50\"\ntouch_precise_amount \"0.5\"\ntouch_move_indicator \"3\"\ntouch_addbutton \"move\" \"\" \"_move\" 0.000000 0.110000 0.480000 1.000000 255 255 255 150 0\ntouch_addbutton \"look\" \"\" \"_look\" 0.480000 0.110000 1.000000 1.000000 255 255 255 150 0\ntouch_addbutton \"attack_l\" \"touch/gfx/attack\" \"+attack\" 0.040000 0.300000 0.150000 0.500000 255 90 90 190 0 1\ntouch_addbutton \"attack\" \"touch/gfx/attack\" \"+attack\" 0.780000 0.420000 0.920000 0.670000 255 90 90 190 0 1\ntouch_addbutton \"attack2\" \"touch/gfx/attack2\" \"+attack2\" 0.680000 0.250000 0.770000 0.410000 255 255 255 170 0 1\ntouch_addbutton \"jump\" \"touch/gfx/jump\" \"+jump\" 0.910000 0.200000 1.000000 0.380000 255 255 255 160 0 1\ntouch_addbutton \"duck\" \"touch/gfx/duck\" \"+duck\" 0.900000 0.700000 1.000000 0.880000 255 255 255 160 512 1\ntouch_addbutton \"reload\" \"touch/gfx/reload\" \"+reload\" 0.800000 0.720000 0.890000 0.880000 255 255 255 160 0 1\ntouch_addbutton \"use\" \"touch/gfx/use\" \"+use\" 0.920000 0.400000 1.000000 0.540000 255 255 255 150 0 1\ntouch_addbutton \"w1\" \"touch/gfx/w_rifle\" \"slot1\" 0.100000 0.000000 0.160000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"w2\" \"touch/gfx/w_pistol\" \"slot2\" 0.180000 0.000000 0.240000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"w3\" \"touch/gfx/w_knife\" \"slot3\" 0.260000 0.000000 0.320000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"w4\" \"touch/gfx/w_grenade\" \"slot4\" 0.340000 0.000000 0.400000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"w5\" \"touch/gfx/w_c4\" \"slot5\" 0.660000 0.000000 0.720000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"buy\" \"touch/gfx/buy\" \"buy\" 0.460000 0.860000 0.540000 1.000000 255 255 255 150 0 1\ntouch_addbutton \"score\" \"touch/gfx/score\" \"scoreboard\" 0.920000 0.000000 1.000000 0.142222 255 255 255 150 0 1\ntouch_addbutton \"touch_edit\" \"touch/gfx/settings\" \"togglemenu\" 0.460000 0.000000 0.540000 0.142222 255 255 255 150 0 1\ntouch_addbutton \"drop\" \"touch/gfx/drop\" \"drop\" 0.580000 0.000000 0.640000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"chat\" \"touch/gfx/chat_all\" \"messagemode\" 0.000000 0.000000 0.060000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"change_team\" \"touch/gfx/change_team\" \"chooseteam\" 0.000000 0.130000 0.060000 0.240000 255 255 255 150 0 1\ntouch_addbutton \"radio\" \"touch/gfx/radio\" \"showvguimenu 38\" 0.740000 0.000000 0.800000 0.106667 255 255 255 150 0 1\ntouch_addbutton \"walk\" \"touch/gfx/walk\" \"+speed\" 0.180000 0.800000 0.270000 0.960000 255 255 255 140 0 1\ntouch_addbutton \"invprev\" \"touch/gfx/left\" \"invprev\" 0.000000 0.560000 0.060000 0.660000 255 255 255 150 0 1\ntouch_addbutton \"invnext\" \"touch/gfx/right\" \"invnext\" 0.000000 0.680000 0.060000 0.780000 255 255 255 150 0 1\n";

    /** Tanlangan boshqaruvni o'yin ochilishida BIR MARTA qo'llaydi (keyin o'yinchi o'zi o'zgartirsa saqlanib qoladi). */
    private String touchPresetArg() {
        SharedPreferences p = prefs();
        String lay = p.getString("touchlay", "pro");
        if (lay.equals(p.getString("touch_applied", ""))) return "";
        try {
            File dir = new File(baseDir(), "cstrike/touch_presets");
            dir.mkdirs();
            FileOutputStream fo = new FileOutputStream(new File(dir, "boosttop_pro.cfg"));
            fo.write(TOUCH_PRO.getBytes("UTF-8"));
            fo.close();
        } catch (Exception e) {
            return "";
        }
        p.edit().putString("touch_applied", lay).apply();
        return "pro".equals(lay) ? " +exec touch_presets/boosttop_pro.cfg" : " +exec touch_presets/phone_ahsim.cfg";
    }

    // ---------------- sozlamalar: til, sezgirlik, pritsel ----------------
    private static final String[] LANGS = {"uz", "ru", "en"};
    private static final String[] LANG_NAMES = {"O'zbekcha", "Русский", "English"};
    private static final String[] XSIZES = {"auto", "small", "medium", "large"};
    private static final int[] XCOLORS = {0xFF32FA32, 0xFFFA3232, 0xFF3232FA, 0xFFFAFA32, 0xFF32FAFA, 0xFFFFFFFF, 0xFFFA32FA};

    private SharedPreferences prefs() {
        return getSharedPreferences("boosttop", MODE_PRIVATE);
    }

    private static String engineLang(String l) {
        return "ru".equals(l) ? "russian" : ("en".equals(l) ? "english" : "uzbek");
    }

    /** Sozlamalarni o'yin konfiguratsiyasiga yozadi (cstrike/boosttop_settings.cfg va config.cfg). */
    private void writeSettings() {
        try {
            SharedPreferences p = prefs();
            int c = p.getInt("xcolor", XCOLORS[0]);
            String color = ((c >> 16) & 0xff) + " " + ((c >> 8) & 0xff) + " " + (c & 0xff);
            String[][] cv = {
                    {"ui_language", engineLang(lang)},
                    {"sensitivity", String.format(Locale.US, "%.1f", p.getFloat("sens", 3.0f))},
                    {"cl_crosshair_size", p.getString("xsize", "auto")},
                    {"cl_crosshair_color", color},
                    {"xhair_enable", p.getInt("xthick", 2) > 0 ? "1" : "0"},
                    {"xhair_thick", String.valueOf(Math.max(1, p.getInt("xthick", 2)))},
                    {"xhair_size", "small".equals(p.getString("xsize", "auto")) ? "3" : "large".equals(p.getString("xsize", "auto")) ? "8" : "5"},
                    {"xhair_gap", "small".equals(p.getString("xsize", "auto")) ? "1" : "large".equals(p.getString("xsize", "auto")) ? "3" : "2"},
                    {"xhair_pad", "1"},
                    {"xhair_dot", p.getBoolean("xdot", false) ? "1" : "0"},
                    {"xhair_t", "0"},
                    {"xhair_additive", "0"},
                    {"xhair_dynamic_scale", "0"},
                    {"xhair_color", color + " 255"},
                    // nishon barqaror bo'lishi uchun: ekran chayqalmasin, qurol tez almashsin
                    {"cl_bob", "0"},
                    {"cl_bobup", "0"},
                    {"cl_dynamiccrosshair", "0"},
                    {"hud_fastswitch", "1"},
                    // giroskop PRO: titrashni yumshatish, mayda harakatda aniqlik
                    {"gyro_yaw", String.format(Locale.US, "%.1f", p.getFloat("gyrosens", 2.0f))},
                    {"gyro_pitch", String.format(Locale.US, "%.1f", p.getFloat("gyrosens", 2.0f))},
                    {"gyro_roll", "0"},
                    {"gyro_yaw_deadzone", "0.8"},
                    {"gyro_pitch_deadzone", "0.8"},
                    {"gyro_smooth", "0.5"},
                    {"gyro_soft", "1"},
                    {"cl_showfps", p.getBoolean("fps", false) ? "1" : "0"},
                    {"gl_picmip", "low".equals(p.getString("gfx", "high")) ? "1" : "0"},
                    {"r_dynamic", "high".equals(p.getString("gfx", "high")) ? "1" : "0"},
                    {"fps_max", "low".equals(p.getString("gfx", "high")) ? "60" : "100"},
                    {"developer", "0"},
                    // tarmoq: o'qlar aniq hisoblanishi uchun (server o'z chegarasigacha cheklaydi)
                    {"rate", "100000"},
                    {"cl_updaterate", "101"},
                    {"cl_cmdrate", "101"},
                    {"ex_interp", "0.02"},
                    {"cl_lw", "1"},
                    {"cl_lc", "1"},
            };
            File dir = new File(baseDir(), "cstrike");
            if (!dir.exists()) return;
            StringBuilder sb = new StringBuilder();
            for (String[] v : cv) sb.append(v[0]).append(" \"").append(v[1]).append("\"\n");
            if (p.contains("gyro")) sb.append("gyro_enable \"").append(p.getBoolean("gyro", false) ? "1" : "0").append("\"\n");
            String nick = p.getString("nick", "").replace("\"", "").replace(";", "").trim();
            if (nick.length() > 0) sb.append("name \"").append(nick).append("\"\n");
            // serverdagi bt_mobile plagini telefon o'yinchisini shu belgi orqali taniydi
            sb.append("setinfo _bt \"m\"\n");
            FileOutputStream fo = new FileOutputStream(new File(dir, "boosttop_settings.cfg"));
            fo.write(sb.toString().getBytes("UTF-8"));
            fo.close();
            // config.cfg ga ham yozamiz - til menyu ochilishidan oldin o'qilishi uchun
            File cfg = new File(dir, "config.cfg");
            if (!cfg.exists()) return; // birinchi ishga tushishda dvijok o'zi yaratadi
            StringBuilder out = new StringBuilder();
            {
                java.io.BufferedReader br = new java.io.BufferedReader(new java.io.InputStreamReader(new java.io.FileInputStream(cfg), "UTF-8"));
                String line;
                while ((line = br.readLine()) != null) {
                    String tl = line.trim();
                    boolean ours = false;
                    for (String[] v : cv) if (tl.startsWith(v[0] + " ") || tl.equals(v[0])) ours = true;
                    if (p.getString("nick", "").trim().length() > 0 && tl.startsWith("name ")) ours = true;
                    if (tl.startsWith("setinfo _bt")) ours = true;
                    if (p.contains("gyro") && tl.startsWith("gyro_enable ")) ours = true;
                    if (!ours) out.append(line).append('\n');
                }
                br.close();
            }
            out.append(sb);
            FileOutputStream co = new FileOutputStream(cfg);
            co.write(out.toString().getBytes("UTF-8"));
            co.close();
        } catch (Exception ignored) {
        }
    }

    /** Pritsel ko'rinishi (namuna). */
    private class CrossView extends View {
        int color = XCOLORS[0];
        String size = "auto";
        int thick = 2;
        boolean dot = false;
        final Paint paint = new Paint();

        CrossView(Context c) {
            super(c);
        }

        @Override
        protected void onDraw(Canvas cv) {
            cv.drawColor(0xFF2A3A2A);
            float cx = getWidth() / 2f, cy = getHeight() / 2f;
            float len = dp("small".equals(size) ? 6 : "large".equals(size) ? 14 : 10);
            float gap = dp("small".equals(size) ? 3 : "large".equals(size) ? 7 : 5);
            float w = dp(thick <= 0 ? 1.5f : thick * 1.5f);
            paint.setStyle(Paint.Style.FILL);
            // qora hoshiya (PRO pritselda) - har qanday fonda ko'rinadi
            for (int pass = (thick > 0 ? 0 : 1); pass < 2; pass++) {
                float o = pass == 0 ? dp(1) : 0;
                paint.setColor(pass == 0 ? 0xFF000000 : color);
                cv.drawRect(cx - gap - len - o, cy - w / 2 - o, cx - gap + o, cy + w / 2 + o, paint);
                cv.drawRect(cx + gap - o, cy - w / 2 - o, cx + gap + len + o, cy + w / 2 + o, paint);
                cv.drawRect(cx - w / 2 - o, cy - gap - len - o, cx + w / 2 + o, cy - gap + o, paint);
                cv.drawRect(cx - w / 2 - o, cy + gap - o, cx + w / 2 + o, cy + gap + len + o, paint);
                if (dot && thick > 0) cv.drawRect(cx - w / 2 - o, cy - w / 2 - o, cx + w / 2 + o, cy + w / 2 + o, paint);
            }
        }
    }

    private LinearLayout chipRow() {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setPadding(0, dp(6), 0, dp(4));
        return r;
    }

    private void showSettings() {
        final SharedPreferences p = prefs();
        final String[] selLang = {lang};
        final float[] selSens = {p.getFloat("sens", 3.0f)};
        final String[] selSize = {p.getString("xsize", "auto")};
        final int[] selColor = {p.getInt("xcolor", XCOLORS[0])};
        final int[] selThick = {p.getInt("xthick", 2)};

        ScrollView sv = new ScrollView(this);
        final LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(18), dp(14), dp(18), dp(6));
        box.setBackgroundColor(C_CARD);
        sv.addView(box);

        // nik
        box.addView(text(t("nick"), 14, C_GOLD, true));
        final EditText nickEd = new EditText(this);
        nickEd.setSingleLine(true);
        nickEd.setTextColor(C_TEXT);
        nickEd.setHintTextColor(C_MUTED);
        String curNick = p.getString("nick", "");
        if (curNick.length() == 0) curNick = playerName();
        nickEd.setText(curNick);
        nickEd.setHint("Player");
        nickEd.setFilters(new android.text.InputFilter[]{new android.text.InputFilter.LengthFilter(31)});
        box.addView(nickEd);

        // til
        TextView lt = text(t("lang"), 14, C_GOLD, true);
        lt.setPadding(0, dp(10), 0, 0);
        box.addView(lt);
        final LinearLayout langRow = chipRow();
        final Button[] langBtns = new Button[LANGS.length];
        for (int i = 0; i < LANGS.length; i++) {
            final int k = i;
            langBtns[i] = button(LANG_NAMES[i], C_LINE);
            langBtns[i].setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    selLang[0] = LANGS[k];
                    for (int j = 0; j < LANGS.length; j++) langBtns[j].setBackground(round(j == k ? C_ACCENT : C_LINE, 8, 0));
                }
            });
            langBtns[i].setBackground(round(LANGS[i].equals(selLang[0]) ? C_ACCENT : C_LINE, 8, 0));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.setMargins(0, 0, dp(6), 0);
            langRow.addView(langBtns[i], lp);
        }
        box.addView(langRow);

        // sezgirlik
        final TextView sensLabel = text("", 14, C_GOLD, true);
        sensLabel.setPadding(0, dp(12), 0, 0);
        sensLabel.setText(t("sens") + ": " + String.format(Locale.US, "%.1f", selSens[0]));
        box.addView(sensLabel);
        SeekBar sb = new SeekBar(this);
        sb.setMax(95); // 0.5 .. 10.0
        sb.setProgress(Math.round((selSens[0] - 0.5f) * 10));
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int v, boolean u) {
                selSens[0] = 0.5f + v / 10f;
                sensLabel.setText(t("sens") + ": " + String.format(Locale.US, "%.1f", selSens[0]));
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        box.addView(sb);

        // pritsel namunasi
        final CrossView cross = new CrossView(this);
        cross.color = selColor[0];
        cross.size = selSize[0];
        cross.thick = selThick[0];
        cross.dot = p.getBoolean("xdot", false);
        LinearLayout.LayoutParams cvp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(70));
        cvp.setMargins(0, dp(12), 0, 0);
        box.addView(cross, cvp);

        // o'lcham
        TextView xs = text(t("xsize"), 14, C_GOLD, true);
        xs.setPadding(0, dp(10), 0, 0);
        box.addView(xs);
        LinearLayout sizeRow = chipRow();
        final Button[] sizeBtns = new Button[XSIZES.length];
        for (int i = 0; i < XSIZES.length; i++) {
            final int k = i;
            sizeBtns[i] = button(t(XSIZES[i]), C_LINE);
            sizeBtns[i].setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            sizeBtns[i].setBackground(round(XSIZES[i].equals(selSize[0]) ? C_ACCENT : C_LINE, 8, 0));
            sizeBtns[i].setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    selSize[0] = XSIZES[k];
                    cross.size = XSIZES[k];
                    cross.invalidate();
                    for (int j = 0; j < XSIZES.length; j++) sizeBtns[j].setBackground(round(j == k ? C_ACCENT : C_LINE, 8, 0));
                }
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.setMargins(0, 0, dp(5), 0);
            sizeRow.addView(sizeBtns[i], lp);
        }
        box.addView(sizeRow);

        // qalinlik
        TextView xt = text(t("xthick"), 14, C_GOLD, true);
        xt.setPadding(0, dp(10), 0, 0);
        box.addView(xt);
        LinearLayout thickRow = chipRow();
        final Button[] thickBtns = new Button[4];
        for (int i = 0; i < 4; i++) {
            final int k = i;
            thickBtns[i] = button(t("xt" + i), C_LINE);
            thickBtns[i].setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
            thickBtns[i].setBackground(round(i == selThick[0] ? C_ACCENT : C_LINE, 8, 0));
            thickBtns[i].setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    selThick[0] = k;
                    cross.thick = k;
                    cross.invalidate();
                    for (int j = 0; j < 4; j++) thickBtns[j].setBackground(round(j == k ? C_ACCENT : C_LINE, 8, 0));
                }
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.setMargins(0, 0, dp(4), 0);
            thickRow.addView(thickBtns[i], lp);
        }
        box.addView(thickRow);
        final CheckBox dotCb = new CheckBox(this);
        dotCb.setText(t("xdot"));
        dotCb.setTextColor(C_TEXT);
        dotCb.setChecked(p.getBoolean("xdot", false));
        dotCb.setOnCheckedChangeListener(new android.widget.CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(android.widget.CompoundButton b, boolean on) {
                cross.dot = on;
                cross.invalidate();
            }
        });
        box.addView(dotCb);

        // rang
        TextView xc = text(t("xcolor"), 14, C_GOLD, true);
        xc.setPadding(0, dp(10), 0, 0);
        box.addView(xc);
        LinearLayout colorRow = chipRow();
        final View[] sw = new View[XCOLORS.length];
        for (int i = 0; i < XCOLORS.length; i++) {
            final int k = i;
            sw[i] = new View(this);
            sw[i].setBackground(round(XCOLORS[i], 18, XCOLORS[i] == selColor[0] ? Color.WHITE : C_LINE));
            sw[i].setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    selColor[0] = XCOLORS[k];
                    cross.color = XCOLORS[k];
                    cross.invalidate();
                    for (int j = 0; j < XCOLORS.length; j++) {
                        GradientDrawable d = round(XCOLORS[j], 18, j == k ? Color.WHITE : C_LINE);
                        if (j == k) d.setStroke(dp(3), Color.WHITE);
                        sw[j].setBackground(d);
                    }
                }
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(34), dp(34));
            lp.setMargins(0, 0, dp(8), 0);
            colorRow.addView(sw[i], lp);
        }
        box.addView(colorRow);

        // giroskop
        final CheckBox gyroCb = new CheckBox(this);
        gyroCb.setText(t("gyro"));
        gyroCb.setTextColor(C_TEXT);
        gyroCb.setChecked(p.getBoolean("gyro", false));
        box.addView(gyroCb);
        final float[] selGyro = {p.getFloat("gyrosens", 2.0f)};
        final TextView gyroLabel = text("", 13, C_MUTED, false);
        gyroLabel.setText(t("gyrosens") + ": " + String.format(Locale.US, "%.1f", selGyro[0]));
        box.addView(gyroLabel);
        SeekBar gsb = new SeekBar(this);
        gsb.setMax(45); // 0.5 .. 5.0
        gsb.setProgress(Math.round((selGyro[0] - 0.5f) * 10));
        gsb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar s, int v, boolean u) {
                selGyro[0] = 0.5f + v / 10f;
                gyroLabel.setText(t("gyrosens") + ": " + String.format(Locale.US, "%.1f", selGyro[0]));
            }

            @Override
            public void onStartTrackingTouch(SeekBar s) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar s) {
            }
        });
        box.addView(gsb);

        // boshqaruv
        final String[] LAYS = {"pro", "std"};
        final String[] selLay = {p.getString("touchlay", "pro")};
        TextView ct = text(t("ctrl"), 14, C_GOLD, true);
        ct.setPadding(0, dp(10), 0, 0);
        box.addView(ct);
        LinearLayout layRow = chipRow();
        final Button[] layBtns = new Button[LAYS.length];
        for (int i = 0; i < LAYS.length; i++) {
            final int k = i;
            layBtns[i] = button(t("ctrl_" + LAYS[i]), C_LINE);
            layBtns[i].setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            layBtns[i].setBackground(round(LAYS[i].equals(selLay[0]) ? C_ACCENT : C_LINE, 8, 0));
            layBtns[i].setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    selLay[0] = LAYS[k];
                    for (int j = 0; j < LAYS.length; j++) layBtns[j].setBackground(round(j == k ? C_ACCENT : C_LINE, 8, 0));
                }
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.setMargins(0, 0, dp(5), 0);
            layRow.addView(layBtns[i], lp);
        }
        box.addView(layRow);

        // grafika
        final String[] GFX = {"high", "mid", "low"};
        final String[] selGfx = {p.getString("gfx", "high")};
        TextView gt = text(t("gfx"), 14, C_GOLD, true);
        gt.setPadding(0, dp(10), 0, 0);
        box.addView(gt);
        LinearLayout gfxRow = chipRow();
        final Button[] gfxBtns = new Button[GFX.length];
        for (int i = 0; i < GFX.length; i++) {
            final int k = i;
            gfxBtns[i] = button(t("gfx_" + GFX[i]), C_LINE);
            gfxBtns[i].setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            gfxBtns[i].setBackground(round(GFX[i].equals(selGfx[0]) ? C_ACCENT : C_LINE, 8, 0));
            gfxBtns[i].setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    selGfx[0] = GFX[k];
                    for (int j = 0; j < GFX.length; j++) gfxBtns[j].setBackground(round(j == k ? C_ACCENT : C_LINE, 8, 0));
                }
            });
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.setMargins(0, 0, dp(5), 0);
            gfxRow.addView(gfxBtns[i], lp);
        }
        box.addView(gfxRow);

        final CheckBox fpsCb = new CheckBox(this);
        fpsCb.setText(t("fps"));
        fpsCb.setTextColor(C_TEXT);
        fpsCb.setChecked(p.getBoolean("fps", false));
        box.addView(fpsCb);

        new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog)
                .setTitle("⚙ " + t("settings"))
                .setView(sv)
                .setPositiveButton(t("save"), new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface d, int w) {
                        boolean langChanged = !selLang[0].equals(lang);
                        p.edit().putString("nick", nickEd.getText().toString().trim())
                                .putString("gfx", selGfx[0]).putBoolean("fps", fpsCb.isChecked())
                                .putString("lang", selLang[0]).putFloat("sens", selSens[0])
                                .putString("xsize", selSize[0]).putInt("xcolor", selColor[0])
                                .putInt("xthick", selThick[0]).putBoolean("xdot", dotCb.isChecked())
                                .putString("touchlay", selLay[0])
                                .putBoolean("gyro", gyroCb.isChecked()).putFloat("gyrosens", selGyro[0]).apply();
                        if (!selLay[0].equals(p.getString("touch_applied", "")))
                            p.edit().putString("touch_applied", "").apply();
                        lang = selLang[0];
                        writeSettings();
                        toastLike(t("saved"));
                        if (langChanged) recreate();
                    }
                })
                .setNegativeButton(t("cancel"), null)
                .show();
    }

    private void confirmDownload() {
        if (downloading) return;
        new AlertDialog.Builder(this)
                .setMessage(String.format(Locale.US, t("dlwarn"), dataSizeMb))
                .setPositiveButton(t("yes"), new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface d, int w) {
                        startDownload();
                    }
                })
                .setNegativeButton(t("no"), null)
                .show();
    }

    private void startDownload() {
        downloading = true;
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        render();
        new Thread(new Runnable() {
            @Override
            public void run() {
                String error = null;
                HttpURLConnection c = null;
                try {
                    File base = baseDir();
                    if (!base.exists() && !base.mkdirs()) throw new Exception("mkdir " + base);
                    String basePath = base.getCanonicalPath() + File.separator;
                    c = (HttpURLConnection) new URL(dataUrl).openConnection();
                    c.setConnectTimeout(15000);
                    c.setReadTimeout(30000);
                    c.setInstanceFollowRedirects(true);
                    if (c.getResponseCode() != 200) throw new Exception("HTTP " + c.getResponseCode());
                    final long total = c.getContentLength() > 0 ? c.getContentLength() : (long) dataSizeMb * 1024 * 1024;
                    CountingStream in = new CountingStream(c.getInputStream());
                    ZipInputStream zip = new ZipInputStream(in);
                    ZipEntry e;
                    byte[] buf = new byte[65536];
                    long lastUi = 0;
                    while ((e = zip.getNextEntry()) != null) {
                        String name = normalizeEntry(e.getName());
                        if (name == null) continue;
                        File out = new File(base, name);
                        if (!out.getCanonicalPath().startsWith(basePath)) continue; // xavfsizlik
                        if (e.isDirectory()) {
                            out.mkdirs();
                            continue;
                        }
                        File parent = out.getParentFile();
                        if (parent != null) parent.mkdirs();
                        OutputStream os = new FileOutputStream(out);
                        int n;
                        while ((n = zip.read(buf)) > 0) {
                            os.write(buf, 0, n);
                            long now = System.currentTimeMillis();
                            if (now - lastUi > 300) {
                                lastUi = now;
                                final long done = in.count;
                                runOnUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        showProgress(done, total);
                                    }
                                });
                            }
                        }
                        os.close();
                    }
                    zip.close();
                    if (!hasData()) throw new Exception("valve/cstrike?");
                } catch (Exception ex) {
                    error = ex.getClass().getSimpleName() + ": " + ex.getMessage();
                } finally {
                    if (c != null) c.disconnect();
                }
                final String err = error;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        downloading = false;
                        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                        toastLike(err == null ? t("dlok") : t("dlerr") + err);
                        render();
                    }
                });
            }
        }).start();
    }

    /** Zip ichida ortiqcha papka bo'lsa ham (masalan cs16_data/cstrike/...) to'g'ri joyga ochadi. */
    private static String normalizeEntry(String n) {
        n = n.replace('\\', '/');
        String[] roots = {"valve/", "cstrike/"};
        for (String r : roots) {
            if (n.startsWith(r)) return n;
            int k = n.indexOf("/" + r);
            if (k >= 0) return n.substring(k + 1);
        }
        return null;
    }

    private void showProgress(long done, long total) {
        if (dlBar == null || dlText == null) return;
        int pct = (int) Math.min(100, done * 100 / Math.max(1, total));
        dlBar.setProgress(pct);
        dlText.setText(t("dling") + (done / (1024 * 1024)) + " / " + (total / (1024 * 1024)) + " MB (" + pct + "%)");
    }

    private static class CountingStream extends java.io.FilterInputStream {
        volatile long count = 0;

        CountingStream(InputStream in) {
            super(in);
        }

        @Override
        public int read() throws java.io.IOException {
            int r = super.read();
            if (r >= 0) count++;
            return r;
        }

        @Override
        public int read(byte[] b, int off, int len) throws java.io.IOException {
            int r = super.read(b, off, len);
            if (r > 0) count += r;
            return r;
        }
    }

    // ---------------- statistika (boost-top.com) ----------------
    /** Telefonning shifrlangan raqami (asl ANDROID_ID saytga yuborilmaydi). */
    private String deviceHash() {
        try {
            String id = Settings.Secure.getString(getContentResolver(), Settings.Secure.ANDROID_ID);
            if (id == null) id = "unknown";
            java.security.MessageDigest md = java.security.MessageDigest.getInstance("SHA-256");
            byte[] h = md.digest((getPackageName() + "|" + id).getBytes("UTF-8"));
            StringBuilder sb = new StringBuilder();
            for (int i = 0; i < 16; i++) sb.append(String.format(Locale.US, "%02x", h[i] & 0xff));
            return sb.toString();
        } catch (Exception e) {
            return "00000000000000000000000000000000";
        }
    }

    /** O'yindagi nik (cstrike/config.cfg dagi name "..."). */
    private String playerName() {
        try {
            File cfg = new File(baseDir(), "cstrike/config.cfg");
            if (!cfg.exists()) return "";
            java.io.BufferedReader br = new java.io.BufferedReader(new java.io.InputStreamReader(new java.io.FileInputStream(cfg), "UTF-8"));
            String line, name = "";
            while ((line = br.readLine()) != null) {
                line = line.trim();
                if (line.startsWith("name ")) {
                    name = line.substring(5).trim();
                    if (name.startsWith("\"") && name.endsWith("\"") && name.length() >= 2) name = name.substring(1, name.length() - 1);
                }
            }
            br.close();
            return name.length() > 32 ? name.substring(0, 32) : name;
        } catch (Exception e) {
            return "";
        }
    }

    /** Fonda saytga xabar yuboradi: ilova ochildi yoki serverga ulandi. */
    private void sendStat(final String action, final String ip) {
        final String dev = deviceHash();
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    String url = STATS_URL + "?a=" + action + "&d=" + dev;
                    if (ip != null) url += "&ip=" + java.net.URLEncoder.encode(ip, "UTF-8");
                    String nick = playerName();
                    if (nick.length() > 0) url += "&n=" + java.net.URLEncoder.encode(nick, "UTF-8");
                    httpGet(url);
                } catch (Exception ignored) {
                }
            }
        }).start();
    }

    // ---------------- tarmoq ----------------
    private static String httpGet(String url) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(8000);
        c.setReadTimeout(10000);
        c.setUseCaches(false);
        try {
            InputStream in = c.getInputStream();
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) > 0) bo.write(buf, 0, n);
            in.close();
            return bo.toString("UTF-8");
        } finally {
            c.disconnect();
        }
    }

    private void loadConfig() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    JSONObject j = new JSONObject(httpGet(CONFIG_URL + "?t=" + System.currentTimeMillis()));
                    dataUrl = j.optString("data_url", dataUrl);
                    engineUrl = j.optString("engine_url", engineUrl);
                    siteUrl = j.optString("site", siteUrl);
                    dataSizeMb = j.optInt("data_size_mb", dataSizeMb);
                    apkBuild = j.optInt("apk_build", 0);
                    apkUrl = j.optString("apk_url", "");
                    Object nw = j.opt("news");
                    if (nw instanceof JSONArray) news = (JSONArray) nw;
                    else if (nw instanceof JSONObject) news = new JSONArray().put(nw);
                    else news = null;
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            render();
                        }
                    });
                } catch (Exception ignored) {
                }
            }
        }).start();
    }

    // ---------------- sevimli serverlar (telefonda saqlanadi) ----------------
    private JSONObject favs() {
        try {
            return new JSONObject(prefs().getString("favs", "{}"));
        } catch (Exception e) {
            return new JSONObject();
        }
    }

    private boolean isFav(String ip) {
        return favs().has(ip);
    }

    private void toggleFav(String ip, String name) {
        try {
            JSONObject f = favs();
            if (f.has(ip)) f.remove(ip); else f.put(ip, name);
            prefs().edit().putString("favs", f.toString()).apply();
        } catch (Exception ignored) {
        }
        render();
    }

    /** Sevimlilar bo'limi uchun: saytdagi jonli ma'lumot, bo'lmasa saqlangan nom. */
    private JSONObject findServer(String ip, String savedName) {
        JSONArray sections = lastData == null ? null : lastData.optJSONArray("sections");
        if (sections != null) for (int i = 0; i < sections.length(); i++) {
            JSONObject s = sections.optJSONObject(i);
            JSONArray list = s == null ? null : s.optJSONArray("servers");
            if (list != null) for (int k = 0; k < list.length(); k++) {
                JSONObject sv = list.optJSONObject(k);
                if (sv != null && ip.equals(sv.optString("ip"))) return sv;
            }
        }
        JSONObject o = new JSONObject();
        try {
            o.put("ip", ip);
            o.put("name", savedName);
            o.put("online", true);
        } catch (Exception ignored) {
        }
        return o;
    }

    // ---------------- server ichidagi o'yinchilar (A2S_PLAYER) ----------------
    private static class PlayerInfo {
        String name;
        int score;
        float time;
    }

    private static java.util.List<PlayerInfo> queryPlayers(String addr) throws Exception {
        int c = addr.lastIndexOf(':');
        java.net.InetAddress host = java.net.InetAddress.getByName(addr.substring(0, c));
        int port = Integer.parseInt(addr.substring(c + 1));
        java.net.DatagramSocket sock = new java.net.DatagramSocket();
        try {
            sock.setSoTimeout(1500);
            byte[] req = {(byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, 0x55, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF};
            byte[] buf = new byte[4096];
            java.net.DatagramPacket in = null;
            for (int attempt = 0; attempt < 4; attempt++) {
                sock.send(new java.net.DatagramPacket(req, req.length, host, port));
                in = new java.net.DatagramPacket(buf, buf.length);
                try {
                    sock.receive(in);
                } catch (java.net.SocketTimeoutException te) {
                    in = null;
                    continue;
                }
                if (in.getLength() >= 9 && buf[4] == 0x41) { // 'A' - challenge
                    req = new byte[]{(byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, 0x55, buf[5], buf[6], buf[7], buf[8]};
                    in = null;
                    continue;
                }
                break;
            }
            if (in == null || in.getLength() < 6 || buf[4] != 0x44) throw new Exception("no answer"); // 'D'
            java.nio.ByteBuffer bb = java.nio.ByteBuffer.wrap(buf, 0, in.getLength()).order(java.nio.ByteOrder.LITTLE_ENDIAN);
            bb.position(5);
            int count = bb.get() & 0xff;
            java.util.List<PlayerInfo> out = new java.util.ArrayList<PlayerInfo>();
            for (int i = 0; i < count && bb.remaining() > 9; i++) {
                bb.get(); // index
                java.io.ByteArrayOutputStream nb = new java.io.ByteArrayOutputStream();
                byte b;
                while (bb.hasRemaining() && (b = bb.get()) != 0) nb.write(b);
                if (bb.remaining() < 8) break;
                PlayerInfo p = new PlayerInfo();
                byte[] raw = nb.toByteArray();
                String nm = new String(raw, "UTF-8");
                if (nm.indexOf('�') >= 0) nm = new String(raw, "windows-1251");
                p.name = nm;
                p.score = bb.getInt();
                p.time = bb.getFloat();
                out.add(p);
            }
            java.util.Collections.sort(out, new java.util.Comparator<PlayerInfo>() {
                @Override
                public int compare(PlayerInfo a, PlayerInfo b) {
                    return b.score - a.score;
                }
            });
            return out;
        } finally {
            sock.close();
        }
    }

    private void showPlayers(final String ip, final String serverName) {
        final LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(16), dp(8), dp(16), dp(8));
        box.setBackgroundColor(C_CARD);
        final TextView st = text(t("loading").replace("Serverlar", "O'yinchilar"), 13, C_MUTED, false);
        box.addView(st);
        ScrollView sv = new ScrollView(this);
        sv.addView(box);
        new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog)
                .setTitle(serverName)
                .setView(sv)
                .setPositiveButton(t("join"), new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface d, int w) {
                        launchGame(ip);
                    }
                })
                .setNegativeButton(t("close"), null)
                .show();
        new Thread(new Runnable() {
            @Override
            public void run() {
                java.util.List<PlayerInfo> list = null;
                try {
                    list = queryPlayers(ip);
                } catch (Exception ignored) {
                }
                final java.util.List<PlayerInfo> res = list;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        box.removeAllViews();
                        if (res == null) {
                            box.addView(text(t("pl_err"), 13, C_MUTED, false));
                            return;
                        }
                        if (res.isEmpty()) {
                            box.addView(text(t("pl_none"), 13, C_MUTED, false));
                            return;
                        }
                        box.addView(playerLine(t("pl_nick"), t("pl_score"), t("pl_time"), true));
                        for (PlayerInfo p : res) {
                            int m = (int) (p.time / 60);
                            String tm = (m / 60) + ":" + String.format(Locale.US, "%02d", m % 60);
                            box.addView(playerLine(p.name, String.valueOf(p.score), tm, false));
                        }
                    }
                });
            }
        }).start();
    }

    private View playerLine(String a, String b, String c, boolean head) {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setPadding(0, dp(5), 0, dp(5));
        int col = head ? C_GOLD : C_TEXT;
        TextView n = text(a, 13, col, head);
        n.setSingleLine(true);
        n.setEllipsize(TextUtils.TruncateAt.END);
        r.addView(n, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        TextView s = text(b, 13, col, head);
        s.setGravity(Gravity.END);
        r.addView(s, new LinearLayout.LayoutParams(dp(56), ViewGroup.LayoutParams.WRAP_CONTENT));
        TextView t2 = text(c, 13, head ? C_GOLD : C_MUTED, head);
        t2.setGravity(Gravity.END);
        r.addView(t2, new LinearLayout.LayoutParams(dp(56), ViewGroup.LayoutParams.WRAP_CONTENT));
        return r;
    }

    private void showPing(String ip, TextView pv) {
        Integer ms = pings.get(ip);
        if (pv == null) return;
        if (ms == null) {
            pv.setText("… ms");
            pv.setBackground(round(C_LINE, 4, 0));
        } else if (ms < 0) {
            pv.setText("— ms");
            pv.setBackground(round(C_OFF, 4, 0));
        } else {
            pv.setText(ms + " ms");
            pv.setBackground(round(ms < 80 ? 0xFF22A355 : ms < 150 ? 0xFFC9A100 : 0xFFD9443A, 4, 0));
        }
    }

    /** Barcha serverlarga ping o'lchaydi (fon oqimida, bir vaqtda). */
    private void measurePings() {
        if (pinging || lastData == null) return;
        final java.util.LinkedHashSet<String> ips = new java.util.LinkedHashSet<String>();
        JSONArray sections = lastData.optJSONArray("sections");
        if (sections != null) for (int i = 0; i < sections.length(); i++) {
            JSONObject s = sections.optJSONObject(i);
            JSONArray list = s == null ? null : s.optJSONArray("servers");
            if (list != null) for (int k = 0; k < list.length(); k++) {
                JSONObject sv = list.optJSONObject(k);
                if (sv != null && sv.optBoolean("online", false)) ips.add(sv.optString("ip"));
            }
        }
        if (ips.isEmpty()) return;
        pinging = true;
        new Thread(new Runnable() {
            @Override
            public void run() {
                java.util.List<Thread> ts = new java.util.ArrayList<Thread>();
                for (final String ip : ips) {
                    Thread t = new Thread(new Runnable() {
                        @Override
                        public void run() {
                            final int ms = pingServer(ip);
                            pings.put(ip, ms);
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    showPing(ip, pingViews.get(ip));
                                }
                            });
                        }
                    });
                    ts.add(t);
                    t.start();
                }
                for (Thread t : ts) try { t.join(3000); } catch (InterruptedException ignored) { }
                pinging = false;
            }
        }).start();
    }

    /** GoldSrc A2S_INFO so'rovi; eng yaxshi javob vaqti (3 urinish), javob bo'lmasa -1. */
    private static int pingServer(String addr) {
        java.net.DatagramSocket sock = null;
        try {
            int c = addr.lastIndexOf(':');
            java.net.InetAddress host = java.net.InetAddress.getByName(addr.substring(0, c));
            int port = Integer.parseInt(addr.substring(c + 1));
            byte[] q = "\u00ff\u00ff\u00ff\u00ffTSource Engine Query\u0000".getBytes("ISO-8859-1");
            sock = new java.net.DatagramSocket();
            sock.setSoTimeout(1000);
            int best = -1;
            byte[] buf = new byte[1400];
            for (int i = 0; i < 3; i++) {
                long t0 = System.nanoTime();
                sock.send(new java.net.DatagramPacket(q, q.length, host, port));
                try {
                    sock.receive(new java.net.DatagramPacket(buf, buf.length));
                    int ms = (int) ((System.nanoTime() - t0) / 1000000L);
                    if (best < 0 || ms < best) best = ms;
                } catch (java.net.SocketTimeoutException ignored) {
                }
            }
            return best;
        } catch (Exception e) {
            return -1;
        } finally {
            if (sock != null) sock.close();
        }
    }

    private void loadServers() {
        if (status != null) status.setText(t("loading"));
        new Thread(new Runnable() {
            @Override
            public void run() {
                JSONObject data = null;
                try {
                    data = new JSONObject(httpGet(SERVERS_URL + "?t=" + System.currentTimeMillis()));
                } catch (Exception ignored) {
                }
                final JSONObject d = data;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (d != null) lastData = d;
                        render();
                        if (d != null) {
                            pings.clear();
                            measurePings();
                        }
                        if (d == null && status != null) status.setText(t("neterr"));
                    }
                });
            }
        }).start();
    }

    private void loadFlag(final String cc, final String base, final ImageView iv) {
        if (TextUtils.isEmpty(cc)) return;
        Bitmap b = flags.get(cc);
        if (b != null) {
            iv.setImageBitmap(b);
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    HttpURLConnection c = (HttpURLConnection) new URL(base + cc).openConnection();
                    c.setConnectTimeout(8000);
                    c.setReadTimeout(8000);
                    final Bitmap bmp = BitmapFactory.decodeStream(c.getInputStream());
                    c.disconnect();
                    if (bmp == null) return;
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            flags.put(cc, bmp);
                            iv.setImageBitmap(bmp);
                        }
                    });
                } catch (Exception ignored) {
                }
            }
        }).start();
    }

    private void openUrl(String url) {
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url)));
        } catch (Exception ignored) {
        }
    }

    private void toastLike(String msg) {
        android.widget.Toast.makeText(this, msg, android.widget.Toast.LENGTH_LONG).show();
    }

    // ---------------- ekranni chizish ----------------
    private void render() {
        if (content == null) return;
        content.removeAllViews();
        pingViews.clear();
        dlBar = null;
        dlText = null;

        if (apkBuild > myBuild() && apkUrl.length() > 0) content.addView(updateCard());
        if (!setupDone() || downloading) content.addView(setupCard());
        addNews();

        status = text("", 13, C_MUTED, false);
        status.setPadding(dp(4), dp(10), dp(4), dp(4));
        content.addView(status);

        if (lastData == null) {
            status.setText(t("loading"));
            return;
        }
        JSONArray sections = lastData.optJSONArray("sections");
        String flagBase = lastData.optString("flag_url", "http://mmb.boost-top.com/flag.php?c=");
        int shown = 0;
        JSONObject fv = favs();
        if (fv.length() > 0) {
            content.addView(sectionHeader(t("favs"), "fav"));
            java.util.Iterator<String> it = fv.keys();
            int r = 1;
            while (it.hasNext()) {
                String fip = it.next();
                content.addView(serverRow(findServer(fip, fv.optString(fip, fip)), r++, flagBase));
                shown++;
            }
        }
        if (sections != null) {
            for (int i = 0; i < sections.length(); i++) {
                JSONObject s = sections.optJSONObject(i);
                if (s == null) continue;
                JSONArray list = s.optJSONArray("servers");
                if (list == null || list.length() == 0) continue;
                String title = s.optString("title", "");
                JSONObject titles = s.optJSONObject("titles");
                if (titles != null && titles.has(lang)) title = titles.optString(lang, title);
                content.addView(sectionHeader(title, s.optString("key", "")));
                for (int k = 0; k < list.length(); k++) {
                    JSONObject sv = list.optJSONObject(k);
                    if (sv != null) {
                        content.addView(serverRow(sv, k + 1, flagBase));
                        shown++;
                    }
                }
            }
        }
        status.setText(shown == 0 ? t("empty") : "");
        status.setVisibility(shown == 0 ? View.VISIBLE : View.GONE);
    }

    // ---------------- yangiliklar qatori ----------------
    private void addNews() {
        JSONArray arr = news;
        if (arr == null) return;
        String hidden = "," + prefs().getString("news_hidden", "") + ",";
        for (int i = 0; i < arr.length(); i++) {
            JSONObject n = arr.optJSONObject(i);
            if (n == null) continue;
            String id = n.optString("id", "");
            if (id.length() > 0 && hidden.contains("," + id + ",")) continue;
            String msg = n.optString(lang, "");
            if (msg.length() == 0) msg = n.optString("ru", n.optString("uz", n.optString("text", "")));
            if (msg.length() == 0) continue;
            content.addView(newsCard(n, id, msg));
        }
    }

    private View newsCard(JSONObject n, final String id, String msg) {
        int col = C_ACCENT;
        try {
            String c = n.optString("color", "");
            if (c.length() > 0) col = android.graphics.Color.parseColor(c);
        } catch (Exception ignored) {
        }
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.HORIZONTAL);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(14), dp(10), dp(6), dp(10));
        card.setBackground(round(C_CARD, 12, col));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(4), 0, dp(6));
        card.setLayoutParams(lp);
        TextView tv = text(msg, 14, C_TEXT, true);
        card.addView(tv, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        final String server = n.optString("server", "");
        final String url = n.optString("url", "");
        if (server.length() > 0 || url.length() > 0) {
            TextView go = text(server.length() > 0 ? t("join") : "›", server.length() > 0 ? 12 : 22, col, true);
            go.setPadding(dp(8), 0, dp(6), 0);
            card.addView(go);
            card.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (server.length() > 0) launchGame(server);
                    else openUrl(url);
                }
            });
        }
        if (id.length() > 0 && !n.optBoolean("pin", false)) {
            TextView x = text("×", 22, C_MUTED, false);
            x.setPadding(dp(10), 0, dp(8), 0);
            x.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    String h = prefs().getString("news_hidden", "");
                    prefs().edit().putString("news_hidden", h.length() == 0 ? id : h + "," + id).apply();
                    render();
                }
            });
            card.addView(x);
        }
        return card;
    }

    /** Ilovaning build raqami (CI assets/bt_build.txt ga yozadi). */
    private int myBuild() {
        try {
            InputStream in = getAssets().open("bt_build.txt");
            byte[] b = new byte[32];
            int n = in.read(b);
            in.close();
            return Integer.parseInt(new String(b, 0, Math.max(0, n), "UTF-8").trim());
        } catch (Exception e) {
            return Integer.MAX_VALUE; // noma'lum bo'lsa yangilanish taklif qilinmaydi
        }
    }

    private View updateCard() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.HORIZONTAL);
        card.setGravity(Gravity.CENTER_VERTICAL);
        card.setPadding(dp(14), dp(12), dp(14), dp(12));
        card.setBackground(round(0xFF0F3A1E, 12, 0xFF35C46A));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(4), 0, dp(8));
        card.setLayoutParams(lp);
        card.addView(text(t("upd"), 14, C_TEXT, true), new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        if (updating) {
            updText = text(t("dling") + "0%", 13, C_GOLD, true);
            card.addView(updText);
            return card;
        }
        Button b = button(t("updb"), 0xFF22A355);
        b.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                confirmSelfUpdate();
            }
        });
        card.addView(b);
        return card;
    }

    // ---------------- ilova ichida yangilash (PackageInstaller) ----------------
    private volatile boolean updating = false;
    private TextView updText;
    private static final String ACTION_INSTALL = "uz.boosttop.cs16.INSTALL_STATUS";
    private android.content.BroadcastReceiver installReceiver;

    /** Avval ogohlantiramiz: o'rnatishda ilova yopiladi (ko'pchilik buni "otib yubordi" deb o'ylaydi). */
    private void confirmSelfUpdate() {
        if (updating) return;
        try {
            if (Build.VERSION.SDK_INT >= 33
                    && checkSelfPermission("android.permission.POST_NOTIFICATIONS") != android.content.pm.PackageManager.PERMISSION_GRANTED)
                requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 77);
        } catch (Exception ignored) {
        }
        new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog)
                .setTitle(t("upd"))
                .setMessage(t("updwarn"))
                .setPositiveButton(t("updok"), new android.content.DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(android.content.DialogInterface d, int w) {
                        startSelfUpdate();
                    }
                })
                .setNegativeButton(t("cancel"), null)
                .show();
    }

    /** Xato bo'lsa ilovadan chiqarib yubormaymiz - tanlov beramiz. */
    private void showUpdateError(String err) {
        sendStat("upderr", (err == null ? "?" : err) + " | " + Build.MANUFACTURER + " " + Build.MODEL + " | SDK " + Build.VERSION.SDK_INT);
        try {
            new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog)
                    .setTitle(t("upderr"))
                    .setMessage(String.valueOf(err))
                    .setPositiveButton(t("updretry"), new android.content.DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(android.content.DialogInterface d, int w) {
                            startSelfUpdate();
                        }
                    })
                    .setNeutralButton(t("updbrowser"), new android.content.DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(android.content.DialogInterface d, int w) {
                            openUrl(apkUrl);
                        }
                    })
                    .setNegativeButton(t("cancel"), null)
                    .show();
        } catch (Exception e) {
            toastLike(t("dlerr") + err);
        }
    }

    /** Yangi APK saqlanadigan fayl (Android 7+ da content:// orqali beriladi). */
    private File updateApkFile() {
        File dir = Build.VERSION.SDK_INT >= 24 ? new File(getCacheDir(), "upd") : getExternalCacheDir();
        if (dir == null) dir = getCacheDir();
        dir.mkdirs();
        return new File(dir, "update.apk");
    }

    /** Tizim o'rnatuvchisini ochadi: "Yangilash/O'rnatish" -> tugagach "Ochish". */
    private void openInstaller(File apk) {
        try {
            Uri uri;
            if (Build.VERSION.SDK_INT >= 24) uri = Uri.parse("content://" + getPackageName() + ".apk/update.apk");
            else {
                apk.setReadable(true, false);
                uri = Uri.fromFile(apk);
            }
            Intent i = new Intent(Intent.ACTION_VIEW)
                    .setDataAndType(uri, "application/vnd.android.package-archive")
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | 1 /* FLAG_GRANT_READ_URI_PERMISSION */);
            startActivity(i);
        } catch (Exception e) {
            showUpdateError(e.getClass().getSimpleName() + ": " + e.getMessage());
        }
    }

    private void startSelfUpdate() {
        if (updating) return;
        // Android 8+: "noma'lum manbalardan o'rnatish" ruxsati shu ilova uchun kerak
        if (Build.VERSION.SDK_INT >= 26 && !getPackageManager().canRequestPackageInstalls()) {
            toastLike(t("updperm"));
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                openUrl(apkUrl);
            }
            return;
        }
        updating = true;
        render();
        final String url = apkUrl;
        new Thread(new Runnable() {
            @Override
            public void run() {
                String err = null;
                HttpURLConnection c = null;
                try {
                    c = (HttpURLConnection) new URL(url).openConnection();
                    c.setConnectTimeout(15000);
                    c.setReadTimeout(30000);
                    c.setInstanceFollowRedirects(true);
                    // GitHub boshqa domenga (https) yo'naltiradi - qo'lda kuzatamiz
                    for (int i = 0; i < 5; i++) {
                        int code = c.getResponseCode();
                        if (code >= 300 && code < 400) {
                            String loc = c.getHeaderField("Location");
                            c.disconnect();
                            c = (HttpURLConnection) new URL(new URL(url), loc).openConnection();
                            c.setConnectTimeout(15000);
                            c.setReadTimeout(30000);
                            continue;
                        }
                        break;
                    }
                    if (c.getResponseCode() != 200) throw new Exception("HTTP " + c.getResponseCode());
                    final long total = Math.max(1, c.getContentLength());
                    // APK ni faylga yuklaymiz, keyin tizimning o'z o'rnatuvchisini ochamiz:
                    // u "O'rnatish" -> "Ochish" tugmalarini ko'rsatadi, o'yinchi nima bo'layotganini ko'radi.
                    final File apk = updateApkFile();
                    OutputStream out = new FileOutputStream(apk);
                    InputStream in = c.getInputStream();
                    byte[] buf = new byte[65536];
                    long done = 0, lastUi = 0;
                    int n;
                    while ((n = in.read(buf)) > 0) {
                        out.write(buf, 0, n);
                        done += n;
                        long now = System.currentTimeMillis();
                        if (now - lastUi > 300) {
                            lastUi = now;
                            final int pct = (int) Math.min(100, done * 100 / total);
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (updText != null) updText.setText(t("dling") + pct + "%");
                                }
                            });
                        }
                    }
                    out.close();
                    in.close();
                    if (c.getContentLength() > 0 && done < c.getContentLength()) throw new Exception("yuklash uzildi (" + done + "/" + c.getContentLength() + ")");
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            updating = false;
                            render();
                            openInstaller(apk);
                        }
                    });
                } catch (Exception e) {
                    err = e.getClass().getSimpleName() + ": " + e.getMessage();
                } finally {
                    if (c != null) c.disconnect();
                }
                final String fe = err;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (fe != null) {
                            updating = false;
                            render();
                            showUpdateError(fe);
                        }
                    }
                });
            }
        }).start();
    }

    /** O'rnatish holati: tizim tasdiq oynasini so'rasa - ochamiz. */
    private void registerInstallReceiver() {
        installReceiver = new android.content.BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                int st = intent.getIntExtra(android.content.pm.PackageInstaller.EXTRA_STATUS, -999);
                if (st == android.content.pm.PackageInstaller.STATUS_PENDING_USER_ACTION) {
                    Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
                    if (confirm != null) {
                        confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        try {
                            startActivity(confirm);
                        } catch (Exception ignored) {
                        }
                    }
                } else if (st != android.content.pm.PackageInstaller.STATUS_SUCCESS) {
                    updating = false;
                    render();
                    String msg = intent.getStringExtra(android.content.pm.PackageInstaller.EXTRA_STATUS_MESSAGE);
                    if (st != android.content.pm.PackageInstaller.STATUS_FAILURE_ABORTED)
                        showUpdateError((msg == null ? "status " + st : msg));
                }
            }
        };
        android.content.IntentFilter f = new android.content.IntentFilter(ACTION_INSTALL);
        if (Build.VERSION.SDK_INT >= 33) registerReceiver(installReceiver, f, 4 /* RECEIVER_NOT_EXPORTED */);
        else registerReceiver(installReceiver, f);
    }

    @Override
    protected void onDestroy() {
        try {
            if (installReceiver != null) unregisterReceiver(installReceiver);
        } catch (Exception ignored) {
        }
        super.onDestroy();
    }

    private View setupCard() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(14), dp(12), dp(14), dp(14));
        card.setBackground(round(C_CARD, 12, C_GOLD));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, dp(4), 0, dp(8));
        card.setLayoutParams(lp);
        card.addView(text(t("setup"), 16, C_GOLD, true));

        boolean data = hasData();
        card.addView(step(t("s3"), (data || downloading) ? null : t("s3b"), data, new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                confirmDownload();
            }
        }));
        if (downloading) {
            dlBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
            dlBar.setMax(100);
            card.addView(dlBar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(18)));
            dlText = text(t("dling") + "...", 12, C_MUTED, false);
            card.addView(dlText);
        }
        return card;
    }

    private View step(String label, String btn, boolean done, View.OnClickListener click) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(10), 0, 0);
        TextView tv = text(label, 13, done ? C_MUTED : C_TEXT, false);
        row.addView(tv, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        if (done) {
            row.addView(text(t("done"), 13, 0xFF35C46A, true));
        } else if (btn != null) {
            Button b = button(btn, C_ACCENT);
            b.setOnClickListener(click);
            row.addView(b);
        }
        return row;
    }

    private View sectionHeader(String title, String key) {
        TextView h = text(title.toUpperCase(Locale.ROOT), 14, Color.WHITE, true);
        h.setLetterSpacing(0.08f);
        h.setPadding(dp(4), dp(14), dp(4), dp(6));
        int color = "top".equals(key) ? 0xFFFF8A00 : "boost".equals(key) ? 0xFF22B8E0 : "vip".equals(key) ? 0xFFFFCC33 : 0xFFEC4899;
        h.setTextColor(color);
        return h;
    }

    private View serverRow(JSONObject sv, int rank, String flagBase) {
        final String ip = sv.optString("ip");
        boolean online = sv.optBoolean("online", false);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(10), dp(10), dp(10), dp(10));
        row.setBackground(round(C_CARD, 10, C_LINE));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, 0, 0, dp(8));
        row.setLayoutParams(lp);

        TextView rk = text(String.valueOf(rank), 14, rank == 1 ? 0xFF1A1200 : C_TEXT, true);
        rk.setGravity(Gravity.CENTER);
        rk.setBackground(round(rank == 1 ? C_GOLD : rank == 2 ? 0xFFC9D3E0 : rank == 3 ? 0xFFD98A4E : C_LINE, 6, 0));
        if (rank == 2 || rank == 3) rk.setTextColor(0xFF10161F);
        row.addView(rk, new LinearLayout.LayoutParams(dp(28), dp(28)));

        LinearLayout info = new LinearLayout(this);
        info.setOrientation(LinearLayout.VERTICAL);
        info.setPadding(dp(10), 0, dp(8), 0);

        LinearLayout nameLine = new LinearLayout(this);
        nameLine.setOrientation(LinearLayout.HORIZONTAL);
        nameLine.setGravity(Gravity.CENTER_VERTICAL);
        ImageView flag = new ImageView(this);
        flag.setAdjustViewBounds(true);
        flag.setScaleType(ImageView.ScaleType.FIT_CENTER);
        LinearLayout.LayoutParams flp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(13));
        flp.setMargins(0, 0, dp(6), 0);
        nameLine.addView(flag, flp);
        loadFlag(sv.optString("cc", ""), flagBase, flag);
        TextView name = text(sv.optString("name", ip), 14, C_TEXT, true);
        name.setSingleLine(true);
        name.setEllipsize(TextUtils.TruncateAt.END);
        nameLine.addView(name, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        final String nmFinal = sv.optString("name", ip);
        final TextView star = text(isFav(ip) ? "★" : "☆", 20, isFav(ip) ? C_GOLD : C_MUTED, true);
        star.setPadding(dp(8), 0, dp(4), 0);
        star.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                toggleFav(ip, nmFinal);
            }
        });
        nameLine.addView(star);
        info.addView(nameLine);

        LinearLayout meta = new LinearLayout(this);
        meta.setOrientation(LinearLayout.HORIZONTAL);
        meta.setGravity(Gravity.CENTER_VERTICAL);
        meta.setPadding(0, dp(5), 0, 0);
        if (online) {
            meta.addView(pill(sv.optString("map", "").toUpperCase(Locale.ROOT), C_MAP, 0xFF1B0F00));
            meta.addView(pill(sv.optInt("players", 0) + "/" + sv.optInt("max", 0), C_PL, Color.WHITE));
        } else {
            meta.addView(pill(t("offline"), C_OFF, Color.WHITE));
        }
        if (online) {
            TextView pv = pill("…", C_LINE, Color.WHITE);
            pingViews.put(ip, pv);
            showPing(ip, pv);
            meta.addView(pv);
        }
        TextView ipv = text(ip, 11, C_MUTED, false);
        ipv.setSingleLine(true);
        meta.addView(ipv);
        info.addView(meta);
        row.addView(info, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        Button join = button(t("join"), C_ACCENT);
        join.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                launchGame(ip);
            }
        });
        row.addView(join);
        row.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showPlayers(ip, nmFinal);
            }
        });
        return row;
    }

    private TextView pill(String s, int bg, int fg) {
        TextView p = text(s, 10, fg, true);
        p.setBackground(round(bg, 4, 0));
        p.setPadding(dp(6), dp(1), dp(6), dp(1));
        p.setSingleLine(true);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, 0, dp(6), 0);
        p.setLayoutParams(lp);
        return p;
    }
}
