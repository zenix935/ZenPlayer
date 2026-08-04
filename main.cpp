#ifdef _WIN32
#include <Windows.h>
#endif
#include "ZenPlayer.h"

int main(int argc,char *argv[])
{
    // #ifdef _WIN32
    //     AllocConsole();
    //     FILE* file;
    //     freopen_s(&file,"CONOUT$","w",stdout);
    //     freopen_s(&file,"CONOUT$","w",stderr);
    //     freopen_s(&file,"CONIN$","r",stdin);
    // #endif

    QApplication a(argc,argv);
    a.setApplicationName("ZenPlayer");
    ZenPlayer w;
    w.show();
    return a.exec();
}
