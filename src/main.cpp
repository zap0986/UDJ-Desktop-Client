/**
 * Copyright 2011 Kurtis L. Nusbaum
 * 
 * This file is part of UDJ.
 * 
 * UDJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * UDJ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with UDJ.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <QApplication>
#include <QIcon>
#include "LoginDialog.hpp"
#ifdef WIN32
#include <QSslConfiguration>
#include <iostream>
#endif

int main(int argc, char* argv[]){
  QApplication app(argc, argv);
  #ifdef WIN32
  std::cout << "disabling ssl config" << std::endl;
  QSslConfiguration defaultConfig = QSslConfiguration::defaultConfiguration();
  defaultConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
  QSslConfiguration::setDefaultConfiguration(defaultConfig);
  #endif
  QIcon windowIcon("udjlauncher.svg");
  QApplication::setWindowIcon(windowIcon);
  app.setApplicationName("Udj");
  app.setQuitOnLastWindowClosed(true);
  UDJ::LoginDialog loginDialog;
  loginDialog.show(); 
  int toReturn = app.exec();
	return toReturn;
}
