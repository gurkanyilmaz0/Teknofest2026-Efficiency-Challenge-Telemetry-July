import tkinter as tk
from tkinter import messagebox
import serial
import serial.tools.list_ports
import threading
import csv
import os
import math
import time
import string
from datetime import datetime

# ─────────────────────────────────────────
#  HABERLEŞME VE EŞİK AYARLARI
# ─────────────────────────────────────────
BAUD_RATE     = 9600
GUNCELLEME    = 100 # ms

HIZ_ESIK       = 70
SICAKLIK_ESIK = 55
GERILIM_ESIK  = 150 # TEKNOFEST Max Batarya Gerilimi 150V
BATARYA_ESIK  = 20

BG        = "#0a0e14"
CYAN      = "#00e5ff"
CYAN_DIM  = "#007a8a"
RED       = "#ff2244"
ORANGE    = "#ff8800"
GREEN     = "#00ff88"
WHITE     = "#e8f4f8"
GRAY      = "#1e2830"
DARK_GRAY = "#141a20"

# Şartname isterlerine göre senkronize edilmiş veri yapısı
veri = {
    "hiz": 0, 
    "sicaklik": 0.0, 
    "gerilim": 0, 
    "akim": 0.0,
    "batarya": 100, 
    "zaman_ms": 0,
    "enerji_Wh": 0,      
    "max_cell_v": 0.0,    
    "min_cell_v": 0.0
}

son_veri_ms   = 0
ser           = None
csv_dosya_obj = None
csv_writer    = None
csv_yolu      = ""
blink_durum   = [True]
thread_calisiyor = True

def suruculeri_listele():
    surucler = ["Masaustu"]
    for harf in string.ascii_uppercase:
        yol = harf + ":\\"
        if os.path.exists(yol):
            surucler.append(harf + ":\\")
    return surucler

def speedometer_ciz(canvas, hiz, max_hiz=220):
    canvas.delete("all")
    cx, cy, r = 160, 155, 130
    canvas.create_arc(cx-r, cy-r, cx+r, cy+r, start=225, extent=-270, outline=GRAY, width=18, style="arc")
    if hiz > 0:
        extent = -(min(hiz, max_hiz) / max_hiz) * 270
        renk = GREEN if hiz < HIZ_ESIK else RED
        canvas.create_arc(cx-r, cy-r, cx+r, cy+r, start=225, extent=extent, outline=renk, width=6, style="arc")
    for i in range(0, max_hiz+1, 20):
        aci = math.radians(225 - (i / max_hiz) * 270)
        x1, y1 = cx + (r-15)*math.cos(aci), cy - (r-15)*math.sin(aci)
        x2, y2 = cx + r*math.cos(aci), cy - r*math.sin(aci)
        canvas.create_line(x1, y1, x2, y2, fill=CYAN_DIM, width=1)
        if i % 40 == 0:
            xt, yt = cx + (r-30)*math.cos(aci), cy - (r-30)*math.sin(aci)
            canvas.create_text(xt, yt, text=str(i), fill=WHITE, font=("Courier", 8))
    aci = math.radians(225 - (min(hiz, max_hiz) / max_hiz) * 270)
    ix, iy = cx + (r-20)*math.cos(aci), cy - (r-20)*math.sin(aci)
    canvas.create_line(cx, cy, ix, iy, fill=RED, width=3, capstyle="round")
    canvas.create_oval(cx-6, cy-6, cx+6, cy+6, fill=RED, outline="")
    canvas.create_text(cx, cy+45, text=str(hiz), fill=RED if hiz > HIZ_ESIK else CYAN, font=("Courier", 28, "bold"))
    canvas.create_text(cx, cy+72, text="km/h", fill=WHITE, font=("Courier", 10))

def serial_oku():
    global son_veri_ms, csv_dosya_obj, csv_writer, ser, thread_calisiyor
    while thread_calisiyor:
        if ser and ser.is_open:
            try:
                if ser.in_waiting > 0:
                    ham = ser.readline().decode("utf-8", errors="ignore").strip()
                    
                    if "$" in ham:
                        satir = ham[ham.index("$")+1:].strip()
                        parcalar = satir.split(";")
                        
                        if len(parcalar) >= 5:
                            try:
                                zaman_ms = int(parcalar[0])
                                veri["zaman_ms"]   = zaman_ms
                                veri["hiz"]        = int(parcalar[1])
                                
                                # ŞARTNAME DÜZELTMESİ: Miliderece (24400) -> Celcius (24.4) dönüşümü[cite: 1]
                                raw_sicaklik       = float(parcalar[2])
                                veri["sicaklik"]   = raw_sicaklik / 1000.0 if raw_sicaklik > 1000 else raw_sicaklik
                                
                                veri["gerilim"]    = int(parcalar[3])
                                veri["enerji_Wh"]  = int(parcalar[4])
                                
                                son_veri_ms = time.time() * 1000
                                
                                # TEKNOFEST BÖLÜM 3 MADDE 9.2.f - OTOMATİK CSV OLUŞTURMA[cite: 1]
                                if csv_writer is None and ser and ser.is_open:
                                    klasor = app.surucu_var.get()
                                    if klasor == "Masaustu": 
                                        klasor = os.path.join(os.path.expanduser("~"), "Desktop")
                                    
                                    dosya_adi = f"ayyildiz_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
                                    global csv_yolu
                                    csv_yolu = os.path.join(klasor, dosya_adi)
                                    
                                    csv_dosya_obj = open(csv_yolu, "w", newline="", encoding="utf-8")
                                    csv_writer = csv.writer(csv_dosya_obj, delimiter=";")
                                    
                                    # Şartnamede istenen BİREBİR başlık satırı[cite: 1]
                                    csv_writer.writerow(["zaman_ms", "hiz_kmh", "T_bat_C", "V_bat_C", "kalan_enerji_Wh"])
                                    app.kayit_info.config(text=f"Otomatik Kayıt: {dosya_adi}", fg=GREEN)

                                if csv_writer and csv_dosya_obj:
                                    # ŞARTNAME FORMATI: CSV'ye T_bat_C verisi tam sayı miliderece olarak yazılır (örn: 24400)[cite: 1]
                                    t_bat_csv = int(raw_sicaklik) if raw_sicaklik > 1000 else int(veri["sicaklik"] * 1000)
                                    
                                    csv_writer.writerow([
                                        zaman_ms, 
                                        veri["hiz"], 
                                        t_bat_csv, 
                                        veri["gerilim"], 
                                        veri["enerji_Wh"]
                                    ])
                                    csv_dosya_obj.flush()
                            except Exception: 
                                pass
            except Exception: 
                pass
        time.sleep(0.01)

class AyyildizApp:
    def __init__(self, root):
        self.root = root
        self.root.title("AYYILDIZ TEAM - Telemetri İzleme")
        self.root.configure(bg=BG)
        self.root.geometry("1100x680") 
        self.root.resizable(False, False)
        self._ui_olustur()
        
    def _ui_olustur(self):
        header = tk.Frame(self.root, bg=BG)
        header.pack(fill="x", padx=10, pady=10)
        self.tarih_lbl = tk.Label(header, text="--/--/----", fg=CYAN, bg=BG, font=("Courier", 12, "bold"))
        self.tarih_lbl.pack(side="left")
        tk.Label(header, text="AYYILDIZ TEAM", fg=WHITE, bg=BG, font=("Courier", 24, "bold")).pack(side="left", expand=True)
        sag_ust = tk.Frame(header, bg=BG)
        sag_ust.pack(side="right")
        self.baglanti_lbl = tk.Label(sag_ust, text="BAĞLI DEĞİL", fg=RED, bg=BG, font=("Courier", 10, "bold"))
        self.baglanti_lbl.pack(anchor="e")
        self.sinyal_lbl = tk.Label(sag_ust, text="Sinyal: --", fg=CYAN_DIM, bg=BG, font=("Courier", 9))
        self.sinyal_lbl.pack(anchor="e")

        ctrl = tk.Frame(self.root, bg=DARK_GRAY, height=40)
        ctrl.pack(fill="x", padx=10, pady=5)

        tk.Label(ctrl, text="PORT:", fg=WHITE, bg=DARK_GRAY, font=("Courier", 9)).pack(side="left", padx=5)
        portlar = [p.device for p in serial.tools.list_ports.comports()]
        self.port_var = tk.StringVar(value="COM5" if "COM5" in portlar else (portlar[0] if portlar else "COM1"))
        tk.OptionMenu(ctrl, self.port_var, *(portlar if portlar else ["COM1"])).pack(side="left", padx=2)
        
        self.btn_baglan = tk.Button(ctrl, text="BAĞLAN", bg=CYAN, fg=BG, font=("Courier", 9, "bold"), command=self._baglan)
        self.btn_baglan.pack(side="left", padx=5)
        
        self.btn_kes = tk.Button(ctrl, text="BAĞLANTI KES", bg=RED, fg=WHITE, font=("Courier", 9, "bold"), command=self._baglanti_kes, state="disabled")
        self.btn_kes.pack(side="left", padx=5)

        tk.Label(ctrl, text=" | KAYIT KONUMU:", fg=WHITE, bg=DARK_GRAY, font=("Courier", 9)).pack(side="left", padx=5)
        self.surucu_var = tk.StringVar(value=suruculeri_listele()[0])
        tk.OptionMenu(ctrl, self.surucu_var, *suruculeri_listele()).pack(side="left", padx=2)
        
        self.kayit_info = tk.Label(ctrl, text="Veri Bekleniyor (Otomatik Kayıt)", fg=CYAN_DIM, bg=DARK_GRAY, font=("Courier", 8))
        self.kayit_info.pack(side="left", padx=10)

        self.durum_lbl = tk.Label(self.root, text="SİSTEM HAZIR", fg=GREEN, bg=BG, font=("Courier", 16, "bold"))
        self.durum_lbl.pack(pady=10)

        ana_frame = tk.Frame(self.root, bg=BG)
        ana_frame.pack(fill="both", expand=True, padx=20)

        # SOL FRAME - Sıcaklık ve Akım Paneli
        f_sol = tk.Frame(ana_frame, bg=BG); f_sol.pack(side="left", fill="y", expand=True)
        self.sic_canv = tk.Canvas(f_sol, width=40, height=140, bg=DARK_GRAY, highlightthickness=1, highlightbackground=CYAN_DIM); self.sic_canv.pack(pady=5)
        self.sic_text = tk.Label(f_sol, text="0.0 C", fg=CYAN, bg=BG, font=("Courier", 14, "bold")); self.sic_text.pack()
        tk.Label(f_sol, text="MAKS. HÜCRE SIC.", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack(pady=(0, 10))

        self.akim_canv = tk.Canvas(f_sol, width=40, height=140, bg=DARK_GRAY, highlightthickness=1, highlightbackground=CYAN_DIM); self.akim_canv.pack(pady=5)
        self.akim_text = tk.Label(f_sol, text="0.0 A", fg=CYAN, bg=BG, font=("Courier", 14, "bold")); self.akim_text.pack()
        tk.Label(f_sol, text="AKIM (A)", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack()

        # ORTA FRAME - Hız Kadranı ve Batarya Paneli
        f_orta = tk.Frame(ana_frame, bg=BG); f_orta.pack(side="left", fill="both", expand=True)
        self.speedo_canv = tk.Canvas(f_orta, width=320, height=300, bg=BG, highlightthickness=0); self.speedo_canv.pack()
        self.bat_canv = tk.Canvas(f_orta, width=280, height=25, bg=DARK_GRAY, highlightthickness=1, highlightbackground=CYAN_DIM); self.bat_canv.pack(pady=5)
        
        self.bat_text = tk.Label(f_orta, text="%100", fg=CYAN, bg=BG, font=("Courier", 12, "bold")); self.bat_text.pack()
        self.wh_text = tk.Label(f_orta, text="0 Wh", fg=GREEN, bg=BG, font=("Courier", 14, "bold")); self.wh_text.pack(pady=2)
        tk.Label(f_orta, text="KALAN ENERJİ MİKTARI", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack()

        # SAĞ FRAME - Gerilim ve Hücre Voltajları Paneli
        f_sag = tk.Frame(ana_frame, bg=BG); f_sag.pack(side="left", fill="y", expand=True)
        self.ger_canv = tk.Canvas(f_sag, width=40, height=140, bg=DARK_GRAY, highlightthickness=1, highlightbackground=CYAN_DIM); self.ger_canv.pack(pady=5)
        self.ger_text = tk.Label(f_sag, text="0 V", fg=CYAN, bg=BG, font=("Courier", 14, "bold")); self.ger_text.pack()
        tk.Label(f_sag, text="TOPLAM GERİLİM", fg=CYAN_DIM, bg=BG, font=("Courier", 9)).pack(pady=(0, 10))
        
        f_hucreler = tk.LabelFrame(f_sag, text="HÜCRE GERİLİMLERİ", fg=CYAN_DIM, bg=DARK_GRAY, font=("Courier", 9), padx=5, pady=5)
        f_hucreler.pack(fill="x", pady=10)
        
        self.max_cell_lbl = tk.Label(f_hucreler, text="Max: 0.00 (V)", fg=WHITE, bg=DARK_GRAY, font=("Courier", 10, "bold"))
        self.max_cell_lbl.pack(anchor="w", pady=4)
        
        self.min_cell_lbl = tk.Label(f_hucreler, text="Min: 0.00 (V)", fg=WHITE, bg=DARK_GRAY, font=("Courier", 10, "bold"))
        self.min_cell_lbl.pack(anchor="w", pady=4)

        threading.Thread(target=serial_oku, daemon=True).start()
        self._alarm_thread()
        self._guncelle()

    def _baglan(self):
        global ser
        try:
            ser = serial.Serial(self.port_var.get(), BAUD_RATE, timeout=0.1)
            self.baglanti_lbl.config(text=f"BAĞLI: {self.port_var.get()}", fg=GREEN)
            self.btn_baglan.config(state="disabled")
            self.btn_kes.config(state="normal")
        except Exception as e:
            messagebox.showerror("Hata", f"Bağlantı hatası: {e}")

    def _baglanti_kes(self):
        global ser, csv_dosya_obj, csv_writer
        if ser and ser.is_open:
            ser.close()
            self.baglanti_lbl.config(text="BAĞLANTI KESİLDİ", fg=ORANGE)
            self.btn_baglan.config(state="normal")
            self.btn_kes.config(state="disabled")
            
            if csv_dosya_obj:
                csv_dosya_obj.close()
                csv_dosya_obj = None
                csv_writer = None
                self.kayit_info.config(text="Kayıt Durduruldu (Bağlantı Kesildi)", fg=RED)

    def _bar_ciz(self, canvas, deger, maks, esik, ters=False):
        canvas.delete("all")
        w, h = int(canvas["width"]), int(canvas["height"])
        oran = max(0, min(deger / maks, 1.0))
        renk = RED if ((deger < esik) if ters else (deger > esik)) else GREEN
        if w > h:
            canvas.create_rectangle(2, 2, int(2+(w-4)*oran), h-2, fill=renk, outline="")
        else:
            dh = int(h * oran)
            canvas.create_rectangle(0, h-dh, w, h, fill=renk, outline="")

    def _alarm_thread(self):
        def _run():
            while thread_calisiyor:
                gecen_sure = time.time() * 1000 - son_veri_ms if son_veri_ms > 0 else 99999
                
                # ŞARTNAME VE ARAÇ YAZILIMI İLE SENKRON KOPUKLUK MANTIĞI[cite: 1]
                if son_veri_ms > 0 and gecen_sure > 3000: 
                    msg = "TELEMETRİ KOPUK - ARAÇ İÇİ SAKLAMA AKTİF"
                    self.durum_lbl.config(text=msg, fg=RED if blink_durum[0] else ORANGE)
                    blink_durum[0] = not blink_durum[0]
                else:
                    h = veri["hiz"] > HIZ_ESIK
                    s = veri["sicaklik"] > SICAKLIK_ESIK
                    g = veri["gerilim"] > GERILIM_ESIK
                    b = veri["batarya"] < BATARYA_ESIK
                    
                    if h or s or g or b:
                        uyarilar = []
                        if h: uyarilar.append("HIZ")
                        if s: uyarilar.append("ISI")
                        if g: uyarilar.append("VOLT")
                        if b: uyarilar.append("BATARYA")
                        
                        msg = "KRİTİK: " + " | ".join(uyarilar)
                        self.durum_lbl.config(text=msg, fg=RED if blink_durum[0] else ORANGE)
                        blink_durum[0] = not blink_durum[0]
                    else:
                        self.durum_lbl.config(text="SİSTEM NORMAL", fg=GREEN)
                time.sleep(0.5)
        threading.Thread(target=_run, daemon=True).start()

    def _guncelle(self):
        speedometer_ciz(self.speedo_canv, veri["hiz"])
        self._bar_ciz(self.sic_canv, veri["sicaklik"], 80, SICAKLIK_ESIK)
        self._bar_ciz(self.akim_canv, veri["akim"], 100, 50)
        self._bar_ciz(self.ger_canv, veri["gerilim"], 150, GERILIM_ESIK)
        self._bar_ciz(self.bat_canv, veri["batarya"], 100, BATARYA_ESIK, ters=True)
        
        self.sic_text.config(text=f"{veri['sicaklik']:.1f} C", fg=RED if veri['sicaklik'] > SICAKLIK_ESIK else CYAN)
        self.akim_text.config(text=f"{veri['akim']:.1f} A", fg=CYAN)
        self.ger_text.config(text=f"{veri['gerilim']} V", fg=RED if veri['gerilim'] > GERILIM_ESIK else CYAN)
        self.bat_text.config(text=f"%{veri['batarya']}", fg=RED if veri['batarya'] < BATARYA_ESIK else CYAN)
        self.wh_text.config(text=f"{veri['enerji_Wh']} Wh", fg=GREEN)
        
        self.max_cell_lbl.config(text=f"Max: {veri['max_cell_v']:.2f} (V)")
        self.min_cell_lbl.config(text=f"Min: {veri['min_cell_v']:.2f} (V)")
        
        now = datetime.now()
        self.tarih_lbl.config(text=now.strftime("%d/%m/%Y | %H:%M:%S"))
        
        if son_veri_ms > 0:
            gecen = (time.time() * 1000 - son_veri_ms) / 1000
            self.sinyal_lbl.config(text=f"Sinyal: {gecen:.1f}s önce", fg=RED if gecen > 3 else GREEN)
        self.root.after(GUNCELLEME, self._guncelle)

if __name__ == "__main__":
    root = tk.Tk()
    app = AyyildizApp(root)
    root.mainloop()
    thread_calisiyor = False
    if csv_dosya_obj:
        csv_dosya_obj.close()