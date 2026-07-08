#include <Windows.h>
#include "ZenPlayer.h"

int main(int argc,char *argv[])
{
    QApplication a(argc,argv);
    ZenPlayer w;
    w.show();
    return a.exec();
}
