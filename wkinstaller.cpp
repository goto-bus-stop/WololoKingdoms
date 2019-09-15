#include "wkinstaller.h"
#include "libwololokingdoms/caseless.h"
#include <QProcess>
#include <QString>
#include <cgenie/scx.h>

WKInstaller::WKInstaller(WKSettings& settings) : settings(settings) {}

void WKInstaller::process() {
  auto postInstall = [this]() {
    auto rmsDir = cfs::resolve(this->settings.hdPath/"resources/_common/drs/gamedata_x2");
    auto scx = cgscx_load((rmsDir/"special_map_yinyang.scx").c_str());
    if (scx == nullptr) printf("reading scx failed: %s\n", (rmsDir/"special_map_yinyang.scx").c_str());
    printf("convert %d\n", cgscx_convert_hd_to_wk(scx));
    printf("save %d\n", cgscx_save(scx, "/tmp/test.scx"));
  };

  postInstall();
  emit finished();
  return;

  converter = std::make_unique<WKConverter>(settings, this);
  try {
    converter->run();
  } catch (const std::exception& e) {
    error(e);
    emit setInfo("error");
    // Retry once…
    try {
      converter->retryInstall();
    } catch (const std::exception& e) {
      error(e, true);

      postInstall();

      emit finished();
      emit setInfo("error");
    }
  }
}

void WKInstaller::error(std::exception const& err, bool showDialog) { 
  std::string errorMessage = err.what();
  if (showDialog)
	emit createDialog("dialogError$" + errorMessage, "Error");
  emit log(errorMessage); 
}

void WKInstaller::installUserPatch(fs::path exePath,
                                   std::vector<std::string> cliFlags) {
  QProcess process;
  QStringList args;
  QString name = QString::fromStdString(exePath.string());

#ifndef _WIN32
  // Use wine on non-windows
  args << name;
  name = QString("wine");
#endif

  for (auto arg : cliFlags) {
    args << QString::fromStdString(arg);
  }

  process.start(name, args);
  process.waitForFinished(180000);
}
