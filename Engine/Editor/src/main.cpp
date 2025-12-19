#include "Aetherion/Editor/EditorApplication.h"
#include <qcoreapplication.h>

int main(int argc, char** argv)
{
    const char* appVersion =
#ifdef AETHERION_APP_VERSION
        AETHERION_APP_VERSION;
#else
        "dev";
#endif
    QCoreApplication::setApplicationVersion(appVersion);
    QCoreApplication::setApplicationName("Aetherion Editor");
    QCoreApplication::setOrganizationName("Aetherion");
    try
    {
        Aetherion::Editor::EditorApplication app(argc, argv);
        return app.Run();
    }
    catch (const std::exception& ex)
    {
        fprintf(stderr, "Fatal error: %s\n", ex.what());
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "Fatal error: unknown exception\n");
        return 1;
    }
}
