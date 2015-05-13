#include <thread>
#include <QApplication>

#include "GLideNUI.h"
#include "AboutDialog.h"
#include "ConfigDialog.h"
#include "Settings.h"

#ifndef _DEBUG
#include <QtPlugin>
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

inline void initMyResource() { Q_INIT_RESOURCE(icon); }
inline void cleanMyResource() { Q_CLEANUP_RESOURCE(icon); }

static
int openConfigDialog()
{
	cleanMyResource();
	initMyResource();
	loadSettings();

	int argc = 0;
	char * argv = 0;
	QApplication a(argc, &argv);

	ConfigDialog w;
	w.show();
	return a.exec();
}

static
int openAboutDialog()
{
	cleanMyResource();
	initMyResource();

	int argc = 0;
	char * argv = 0;
	QApplication a(argc, &argv);

	AboutDialog w;
	w.show();
	return a.exec();
}

int runConfigThread() {
	std::thread configThread(openConfigDialog);
	configThread.join();
	return 0;
}

int runAboutThread() {
	std::thread aboutThread(openAboutDialog);
	aboutThread.join();
	return 0;
}

EXPORT int CALL RunConfig()
{
	return runConfigThread();
}

EXPORT int CALL RunAbout()
{
	return runAboutThread();
}

EXPORT void CALL LoadConfig()
{
	loadSettings();
}
