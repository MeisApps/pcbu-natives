#ifndef PAM_PCBIOUNLOCK_KEYSCANNER_H
#define PAM_PCBIOUNLOCK_KEYSCANNER_H

#include <map>
#include <vector>
#include <thread>

#include "utils/Utils.h"

class KeyScanner {
public:
    KeyScanner();
    ~KeyScanner();

    bool GetKeyState(int key);
    std::map<int, bool> GetAllKeys();

    void Start();
    void Stop();

private:
#ifndef _WIN32
    static std::vector<std::string> GetKeyboards();
    void ScanThread();

    std::thread m_ScanThread;
    bool m_IsRunning{};

    std::map<int, bool> m_KeyMap;
#endif
};


#endif //PAM_PCBIOUNLOCK_KEYSCANNER_H
