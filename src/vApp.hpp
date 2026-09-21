#ifndef VAPP_HPP
#define VAPP_HPP


#include "ConsoleManager.hpp" // Pentru funcționalitățile de logare


#include <string>    // Pentru std::string
#include <map>

//#include <commctrl.h>
//#pragma comment(lib, "comctl32.lib")
// Declarații forward (utile, dar nu strică să fie listate explicit include-urile).

enum class RunMode { GUI, CONSOLE, SERVICE };

class vApp {
    
public:
    // Constructorul inițializează aplicația cu handle-ul instanței.
    // Setează pointerul static s_instance pentru delegarea globală a mesajelor.
    explicit vApp(HINSTANCE hInstance, RunMode mode = RunMode::CONSOLE);

    // Destructorul implicit este suficient, deoarece unique_ptr gestionează memoria.
    // virtual este o bună practică pentru clasele de bază.
    virtual ~vApp() = default;

    // Bucla principală a aplicației.
    // Returnează codul de ieșire al aplicației.
    int run(int nCmdShow = SW_SHOW);

    // Inițializează componentele aplicației (ferestre, panouri, butoane, handleri).
    // Returnează true la inițializare reușită, false altfel.
    bool init();
    
    virtual bool initConsole() { return true; }
    virtual bool initService() { return true; }

    // Handlerul principal de mesaje pentru ferestrele aplicației.
    // Această metodă este apelată de către WndProc-ul static.
    // NOTĂ: Această metodă ar trebui să fie delegatorul final pentru mesajele la nivel de aplicație
    // care nu sunt gestionate de o fereastră specifică (e.g., WM_QUIT, WM_APPCOMMAND).
    // Mesajele specifice ferestrelor ar trebui să fie tratate de `vWindow::handleMessage`.
   

    // Accesor pentru handle-ul instanței aplicației.
    HINSTANCE getInstance() const { return m_instance; }

    static vApp* getAppInstance() { return s_instance; }
    // Returnează HWND-ul ferestrei principale.
    // Presupune că o fereastră cu ID-ul "main" există întotdeauna și are un handle valid.
   

    // Returnează un pointer (care nu deține proprietatea) către o fereastră după ID-ul său.
    // Returnează nullptr dacă nu este găsită nicio fereastră cu ID-ul respectiv.
   

    // Adaugă o nouă fereastră în manager. Preia proprietatea asupra unique_ptr.
   
    // Oprește aplicația, efectuând curățenia necesară.
    void shutdown();

    //Porneste consola
    void startConsole();

    // O metodă de test, care în prezent declanșează evenimentul 'onClose' pentru fereastra principală.
    void test();

   

    void setGlobalVar(const std::wstring& key, const std::wstring& value) {
        m_globalVars[key] = value;
    }

    std::wstring getGlobalVar(const std::wstring& key) const {
        auto it = m_globalVars.find(key);
        return (it != m_globalVars.end()) ? it->second : L"";
    }


    // Returnează calea folderului unde se află executabilul (cu '\' la final), ex: L"D:\\Programming\\ANC\\"
    std::wstring getAppPath() const;

    // Versiune std::string (ANSI / UTF-8) pentru getAppPath()
    std::string getAppPathA() const;

    // Returnează calea completă către o resursă/subfolder relativ la directorul aplicației
    // Exemplu: getAppSubPath(L"reports/situatii_periodice/situatii_periodice.xml")
    std::wstring getAppSubPath(const std::wstring& relativePath) const;
    std::string getAppSubPathA(const std::string& relativePath) const;

protected:
    // Gestionează proprietatea și ciclul de viață al obiectelor vWindow.
    
    HINSTANCE m_instance;         // Handle-ul instanței aplicației

    void setRunMode(RunMode mode) { m_runMode = mode; }

    std::map<std::wstring, std::wstring> m_globalVars;
private:
   

    // Procedura statică de fereastră pentru gestionarea mesajelor WinAPI.
    // Deleagă mesajele către metoda non-statică handleMessage a s_instance.
    // Aceasta ar trebui să fie procedura de fereastră (WndProc) principală,
    // înregistrată la WinAPI pentru *clasa de fereastră a ferestrei principale*.
    //static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Pointer static către instanța vApp.
    // Folosit de WndProc-ul static pentru a direcționa mesajele către obiectul vApp corect.
    // Acesta este un singleton la nivel de aplicație.
    static vApp* s_instance;

    
    

    RunMode m_runMode = RunMode::CONSOLE;
};

#endif // VAPP_HPP