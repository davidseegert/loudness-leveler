#include <wx/wx.h>
#include "MainWindow.h"

class MyApp : public wxApp {
public:
    virtual bool OnInit() {
        MainWindow *frame = new MainWindow();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);