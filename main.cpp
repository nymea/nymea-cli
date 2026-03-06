#include <string>

#include <QCommandLineParser>
#include <QCoreApplication>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#ifndef APP_VERSION
#define APP_VERSION "0.1.0"
#endif

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("nymea-cli"));
  QCoreApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Terminal UI application built with Qt6 Core and FTXUI."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.process(app);

  const std::string version = app.applicationVersion().toStdString();
  auto screen = ftxui::ScreenInteractive::Fullscreen();

  auto ui = ftxui::Renderer([&] {
    return ftxui::vbox({
               ftxui::hbox({
                   ftxui::text(" nymea-cli "),
                   ftxui::filler(),
                   ftxui::text("Version " + version),
               }) |
                   ftxui::border,
               ftxui::separator(),
               ftxui::text("Press q or Esc to quit."),
           }) |
           ftxui::border | ftxui::flex;
  });

  auto with_key_handler = ftxui::CatchEvent(ui, [&](ftxui::Event event) {
    if (event == ftxui::Event::Character("q") || event == ftxui::Event::Escape) {
      screen.ExitLoopClosure()();
      return true;
    }
    return false;
  });

  screen.Loop(with_key_handler);
  return 0;
}
