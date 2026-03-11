# 🎯 VolkDMA Example - Real Hardware Testing

Program complet pentru testarea funcțiilor VolkDMA cu placa DMA reală (FPGA).

---

## 📋 **Ce Testează Programul:**

✅ **Initialization** - Inițializare DMA cu FPGA hardware  
✅ **Process Discovery** - Găsire procese din target PC  
✅ **Memory Reading** - Citire memorie (8/16/32/64-bit)  
✅ **Batch Operations** - Citiri multiple eficiente  
✅ **Signature Scanning** - Căutare pattern-uri în memorie  
✅ **Performance Metrics** - Monitoring performanță  
✅ **Health Monitoring** - Status checks sistem

---

## ⚠️ **CERINȚE:**

### **Hardware:**
- ✅ Placă FPGA DMA (PCILeech, Squirrel, etc.)
- ✅ Instalată în PCIe slot
- ✅ Target PC conectat și accesibil

### **Software:**
- ✅ Windows 10/11
- ✅ Visual Studio 2022 (sau Build Tools)
- ✅ Administrator privileges
- ✅ Drivere instalate:
  - `vmm.dll`
  - `leechcore.dll`

### **Librărie:**
- ✅ `VolkDMA.lib` compilată (în `../x64/Release/`)

---

## 🚀 **Cum să Compilezi:**

### **Metoda 1: Build Script (Recomandat)**

```powershell
# Deschide Developer Command Prompt for VS 2022
cd example
.\build.bat
```

### **Metoda 2: Manual**

```powershell
cl /EHsc /std:c++20 /O2 /MD /DNOMINMAX ^
   /I"..\include" ^
   /Fe:test_dma.exe ^
   test_dma.cpp ^
   /link ^
   /LIBPATH:"..\x64\Release" VolkDMA.lib ^
   /LIBPATH:"..\external\vmm" vmm.lib ^
   /LIBPATH:"..\external\leechcore" leechcore.lib
```

---

## 🎮 **Cum să Rulezi:**

### **Pas 1: Verifică Setup-ul**

```powershell
# Verifică dacă DLL-urile există
dir x64\Release\*.dll

# Ar trebui să vezi:
#   vmm.dll
#   leechcore.dll
```

### **Pas 2: Rulează ca Administrator**

```powershell
# Metoda 1: Right-click pe test_dma.exe
Right-click → Run as administrator

# Metoda 2: PowerShell ca Admin
cd x64\Release
.\test_dma.exe
```

---

## 📖 **Ghid de Folosire:**

### **1. Inițializare:**

```
Programul va inițializa automat DMA device-ul.
Dacă vezi "DMA initialized with FPGA hardware!" → SUCCESS!
```

### **2. Process Discovery:**

```
Select option: 2
Enter target process name: notepad.exe

Program va căuta procesul în target PC prin DMA.
```

### **3. Memory Reading:**

```
Select option: 3
Enter memory address: 0x140000000

Program va citi memoria de la adresa specificată.
Afișează valori în format 8/16/32/64-bit.
```

### **4. Batch Operations:**

```
Select option: 4

Program va testa citiri batch (multiple simultan).
Afișează throughput și statistici.
```

### **5. Signature Scanning:**

```
Select option: 5
Enter signature pattern: 48 8B ?? ?? 89

Program va scana memoria pentru pattern-ul specificat.
?? = wildcard (orice valoare)
```

### **6. Performance Metrics:**

```
Select option: 6

Afișează statistici de performanță:
- Total operations
- Success rate
- Throughput (MB/s)
- Average time
```

### **7. Health Monitoring:**

```
Select option: 7

Verifică starea sistemului DMA:
- FPGA connection
- Memory mapping
- Driver status
```

### **8. Run All Tests:**

```
Select option: 8

Rulează toate testele automat.
```

---

## 🐛 **Troubleshooting:**

### **"DMA initialization failed"**

**Cauze posibile:**
- FPGA nu e conectat
- Drivere lipsă sau incorecte
- Nu rulezi ca Administrator

**Soluții:**
1. Verifică conexiunea FPGA (Device Manager)
2. Reinstalează driverele
3. Asigură-te că rulezi ca Admin

---

### **"Process not found"**

**Cauze:**
- Procesul nu rulează pe target PC
- Numele procesului e greșit (case sensitive)
- Target PC nu e accesibil prin DMA

**Soluții:**
1. Verifică că procesul rulează pe target: `tasklist | findstr notepad`
2. Folosește numele exact: `notepad.exe` (nu `Notepad` sau `notepad`)
3. Testează conectivitatea DMA

---

### **"Memory read failed"**

**Cauze:**
- Adresa invalidă
- Proces nu are acea regiune de memorie
- Permisiuni insuficiente

**Soluții:**
1. Folosește adrese valide (ex: module base address)
2. Verifică memory map-ul procesului
3. Încearcă alte adrese

---

### **"DLL not found"**

**Cauze:**
- `vmm.dll` sau `leechcore.dll` lipsesc

**Soluții:**
1. Copiază DLL-urile în `x64\Release\`
2. Sau adaugă path-ul în PATH environment variable
3. Rulează `build.bat` din nou (copiază automat)

---

## 📊 **Output Exemplu:**

```
╔════════════════════════════════════════════╗
║  VolkDMA Real Hardware Test Program        ║
║  Testing DMA Library with FPGA Card        ║
╚════════════════════════════════════════════╝

ℹ Initializing DMA device...
✓ DMA initialized with FPGA hardware!

╔═══════════════════════════════════════╗
║   VolkDMA - Real Hardware Test        ║
║   FPGA DMA Testing & Integration      ║
╚═══════════════════════════════════════╝

⚠ REQUIREMENTS:
  - Run as Administrator
  - FPGA device connected
  - Target PC accessible via DMA
  - vmm.dll and leechcore.dll present

Available Tests:

  1. Initialization Test
  2. Process Discovery
  3. Memory Reading
  4. Batch Operations
  5. Signature Scanning
  6. Performance Metrics
  7. Health Monitoring
  8. Run All Tests
  0. Exit

Select option: 2

========================================
  TEST 2: Process Discovery
========================================

Enter target process name: notepad.exe
ℹ Searching for process: notepad.exe
✓ Process found! PID: 5432
Total instances: 1

Press ENTER to continue...
```

---

## 📝 **Note Importante:**

1. **Administrator Rights:**
   - OBLIGATORIU pentru acces la FPGA
   - Programul nu va funcționa fără

2. **Target PC:**
   - Trebuie să fie accesibil prin DMA
   - Procesele trebuie să ruleze pe target

3. **Adrese de Memorie:**
   - Folosește adrese valide (0x140000000 pentru .text PE section)
   - Poți folosi tools precum Process Hacker pentru a găsi adrese

4. **Signature Patterns:**
   - Format: bytes în hex separate prin spații
   - Wildcard: `??` pentru orice byte
   - Exemplu: `48 8B ?? ?? 89` = `mov rax, [???]; mov [...]`

---

## 🔗 **Resurse Utile:**

- **Documentație:** `../docs/`
- **API Reference:** `../docs/API_REFERENCE.md`
- **Examples:** `../docs/EXAMPLES.md`
- **FAQ:** `../docs/FAQ.md`

---

## 🎯 **Next Steps:**

După ce testezi cu success toate funcțiile, poți:

1. **Integrezi** - Folosește codul ca bază pentru propriul tău program
2. **Extinde** - Adaugă funcții custom specifice use-case-ului tău
3. **Optimizează** - Folosește batch operations și cache pentru performanță maximă

---

**Happy DMA Testing! 🚀**
