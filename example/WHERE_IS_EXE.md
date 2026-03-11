# 🎯 VolkDMA Example - Locația Programelor

## 📂 **Unde Sunt Programele:**

### ✅ **simple_test.exe** - Program Minimal DMA Test

**Locație:** `example\x64\Release\simple_test.exe`

**Status:** ⚠️ **NU E COMPILAT ÎNC**Ă**

---

## 🔨 **Cum să Compilezi:**

### **Metoda 1: Visual Studio x64 Command Prompt (RECOMANDAT)**

```powershell
# 1. Deschide "x64 Native Tools Command Prompt for VS 2022"
#    (Caută în Start Menu)

# 2. Navighează la proiect
cd C:\Users\bujor\Desktop\VolkDMA-main

# 3. Compilează
cl /EHsc /std:c++20 /O2 /MD ^
   /I"external\vmm" /I"external\leechcore" ^
   /Fe:example\x64\Release\simple_test.exe ^
   example\simple_test.cpp ^
   /link ^
   /LIBPATH:"external\vmm" vmm.lib ^
   /LIBPATH:"external\leechcore" leechcore.lib
```

### **Metoda 2: Folosind MSBuild**

```powershell
# În folder-ul proiectului
msbuild example\simple_test.vcxproj /p:Configuration=Release /p:Platform=x64
```

### **Metoda 3: Visual Studio IDE**

1. Deschide `VolkDMA.sln` în Visual Studio
2. Click dreapta pe `simple_test` project → Build
3. Executabilul va fi în `example\x64\Release\`

---

## ⚠️ **Problema Curentă:**

Compilatorul folosit este **x86 (32-bit)** dar librăriile DMA sunt **x64 (64-bit)**.

**Soluție:**
- Folosește **x64 Native Tools Command Prompt** în loc de Command Prompt normal
- SAU compilează din Visual Studio care alege automat arhitectura corectă

---

## 🎯 **Ce Face simple_test.exe:**

Program minimal care:
1. ✅ Inițializează DMA cu FPGA
2. ✅ Listează procese din target PC
3. ✅ Caută un proces specific (ex: notepad.exe)
4. ✅ Citește memorie de la o adresă
5. ✅ Afișează rezultatul

---

## 📝 **Pentru a Rula După Compilare:**

```powershell
cd example\x64\Release

# RIGHT-CLICK pe simple_test.exe → Run as Administrator
# SAU:
.\simple_test.exe
```

**Cerințe:**
- Administrator privileges
- FPGA connected
- Target PC accessible

---

## 🚀 **Alternative:**

Dacă `simple_test.exe` nu compilează, folosește:

### **Librăria VolkDMA Direct:**

```cpp
#include <VolkDMA/dma.hh>

int main() {
    VolkDMA::DMA dma(true);
    DWORD pid = dma.get_process_id("notepad.exe");
    uint32_t value = dma.read<uint32_t>(0x140000000, pid);
    return 0;
}
```

**Compilare:**
```powershell
cl your_program.cpp /link VolkDMA.lib vmm.lib leechcore.lib
```

---

**Pentru Help:** Vezi `example\README.md` pentru detalii complete.
