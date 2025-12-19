#include "Aetherion/Editor/EditorApplication.h"
#include <qcoreapplication.h>

int main(int argc, char** argv)
{
    QCoreApplication::setApplicationVersion("0.0.1 Dev Alpha");
    QCoreApplication::setApplicationName("Aetherion Editor");
    QCoreApplication::setOrganizationName("Aetherion");

    Aetherion::Editor::EditorApplication app(argc, argv);
    return app.Run();
}
