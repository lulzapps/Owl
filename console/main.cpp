// Owl - www.owlclient.com
// Copyright (c) 2012-2017, Adalid Claure <aclaure@gmail.com>

#include <QCoreApplication>
#include <QCommandLineParser>
#include "Core.h"
#include "OwlConsole.h"
#include <Utils/OwlUtils.h>

std::unique_ptr<QCommandLineParser> createCommandLineParser()
{
    std::unique_ptr<QCommandLineParser> parser(new QCommandLineParser);

    parser->addHelpOption();
    parser->addVersionOption();

    parser->addOption({{"l", "lua", "luafolder"}, "The folder to use for Lua folders", "luafolder"});
    parser->addOption({{"f", "file"}, "Load file of line delimited list of commands to execute", "file"});
    parser->addOption({"color", "If true (default) terminal output will be colored. Set to fale to disable", "color"});

    QCommandLineOption cmndsOption(QStringList() << "c" << "command", "Specify a command to execute at startup. Can be used multiple times.", "command");
    parser->addOption(cmndsOption);

    return parser;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    a.setApplicationName(QStringLiteral(APP_NAME));
    a.setOrganizationName(QStringLiteral(ORGANIZATION_NAME));
    a.setOrganizationDomain(QStringLiteral(ORGANZATION_DOMAIN));
    a.setApplicationVersion(QStringLiteral(OWLCONSOLE_VERSION));

    auto parser = createCommandLineParser();
    parser->process(a);

    owl::ConsoleApp mainApp(&a);

    {
        const bool colorOn = !(parser->isSet("color")
                && (parser->value("color").toLower() == "off"
                    || parser->value("color").toLower() == "false"));

        mainApp.setColor(colorOn);
    }

    if (parser->isSet("luafolder"))
    {
        mainApp.setLuaFolder(parser->value("luafolder"));
    }
    else
    {
        const QDir dir(QCoreApplication::applicationDirPath());
        mainApp.setLuaFolder(dir.absoluteFilePath("parsers"));
    }

    if (parser->isSet("file"))
    {
        mainApp.setCommandfile(parser->value("file"));
    }

    const auto startCommands = parser->values("command");
    if (startCommands.size() > 0)
    {
        auto& cs = mainApp.getStartCommands();
        cs = startCommands;
    }

    QObject::connect(&mainApp, SIGNAL(finished()), &a, SLOT(quit()));
    QTimer::singleShot(0, &mainApp, SLOT(run()));
    
    return a.exec();
}
