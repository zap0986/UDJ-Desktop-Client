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
#include "MetaWindow.hpp"
#include "MusicFinder.hpp"
#include "DataStore.hpp"
#include "LibraryWidget.hpp"
#include "ActivityList.hpp"
#include "ActivePlaylistView.hpp"
#include "PlayerCreateDialog.hpp"
#include "PlayerDashboard.hpp"
#include "Logger.hpp"
#include "AboutWidget.hpp"
#include "LogViewer.hpp"
#include "SetLocationDialog.hpp"
#include "SetPasswordDialog.hpp"
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QTabWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMenuBar>
#include <QLabel>
#include <QStackedWidget>
#include <QSplitter>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>


namespace UDJ{


MetaWindow::MetaWindow(
  const QString& username,
  const QString& password,
  const QByteArray& ticketHash,
  const user_id_t& userId,
  QWidget *parent,
  Qt::WindowFlags flags)
  :QMainWindow(parent,flags),
  isQuiting(false)
{
  dataStore = new DataStore(username, password, ticketHash, userId, this);
  createActions();
  setupUi();
  setupMenus();
  QSettings settings(
    QSettings::UserScope,
    DataStore::getSettingsOrg(),
    DataStore::getSettingsApp());
  if(settings.contains("metaWindowGeometry")){
    restoreGeometry(settings.value("metaWindowGeometry").toByteArray());
    restoreState(settings.value("metaWindowState").toByteArray());
  }
  else{
    setWindowState(Qt::WindowMaximized);
  }
  if(dataStore->hasPlayerId()){
    dataStore->setPlayerState(DataStore::getPlayingState());
    if(dataStore->hasUnsyncedSongs()){
      syncLibrary();
    }
  }
  else{
    PlayerCreateDialog *createDialog = new PlayerCreateDialog(dataStore, this);
    createDialog->show();
  }

  connect(
    dataStore,
    SIGNAL(playerPasswordSet()),
    this,
    SLOT(enableRemovePassword()));

  connect(
    dataStore,
    SIGNAL(playerPasswordRemoved()),
    this,
    SLOT(disableRemovePassword()));
  connect(
    dataStore,
    SIGNAL(playerCreated()),
    this,
    SLOT(checkForITunes()));
}

void MetaWindow::closeEvent(QCloseEvent *event){
  if(!isQuiting){
    isQuiting = true;
    connect(
      dataStore,
      SIGNAL(playerStateChanged(const QString&)),
      this,
      SLOT(close()));
    quittingProgress = new QProgressDialog("Disconnecting...", "Cancel", 0, 0, this);
    quittingProgress->setWindowModality(Qt::WindowModal);
    quittingProgress->setMinimumDuration(250);
    dataStore->setPlayerState(DataStore::getInactiveState());
    event->ignore();
  }
  else{
    QSettings settings(
      QSettings::UserScope,
      DataStore::getSettingsOrg(),
      DataStore::getSettingsApp());
    settings.setValue("metaWindowGeometry", saveGeometry());
    settings.setValue("metaWindowState", saveState());
    QMainWindow::closeEvent(event);
  }
}

void MetaWindow::checkForITunes(){
  QString musicDir = QDesktopServices::storageLocation(QDesktopServices::MusicLocation);
  QDir iTunesDir = QDir(musicDir).filePath("iTunes");
  if(iTunesDir.exists("iTunes Music Library.xml")){
    QMessageBox::StandardButton response = QMessageBox::question(
      this, "Import iTunes Library", "Looks like you've got iTunes installed. Would you like"
      " us to try to import your iTunes Library?",
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::Yes);
    if(response == QMessageBox::Yes){
      QList<Phonon::MediaSource> musicToAdd =
        MusicFinder::findItunesMusic(iTunesDir.filePath("iTunes Music Library.xml"));
      Logger::instance()->log("Size of itunes was: " + QString::number(musicToAdd.size()));
      addMediaSources(musicToAdd);
    }
  }
  else{
    Logger::instance()->log(iTunesDir.filePath("iTunes Music Library.xml") + " doesn't exist");
  }
}

void MetaWindow::addMediaSources(const QList<Phonon::MediaSource>& musicToAdd){
  if(musicToAdd.isEmpty()){
    QMessageBox::information(
        this, 
        "No Music Found", 
        "Sorry, but we couldn't find any music that we know how to play.");
    return;
  }

  int numNewFiles = musicToAdd.size();
  QProgressDialog *addingProgress = new QProgressDialog(
    "Loading Library...", "Cancel", 0, numNewFiles, this);
  addingProgress->setWindowModality(Qt::WindowModal);
  addingProgress->setMinimumDuration(250);
  dataStore->addMusicToLibrary(musicToAdd, addingProgress);
  if(!addingProgress->wasCanceled()){
    syncLibrary();
  }
  addingProgress->close();
}

void MetaWindow::addMusicToLibrary(){
  QString musicDir = QFileDialog::getExistingDirectory(this,
    tr("Pick folder to add"),
    QDir::homePath(),
    QFileDialog::ShowDirsOnly);
  Logger::instance()->log("got directory: " + musicDir);
  if(musicDir == ""){
    return;
  }
  QList<Phonon::MediaSource> musicToAdd =
    MusicFinder::findMusicInDir(musicDir);
  addMediaSources(musicToAdd);
}

void MetaWindow::addSongToLibrary(){
  QString fileName = QFileDialog::getOpenFileName(
      this,
      tr("Pick song to add"),
      QDir::homePath(),
      tr("Audio Files ") + MusicFinder::getMusicFileExtFilter());
  if(fileName == ""){
    return;
  }
  QList<Phonon::MediaSource> songList;
  songList.append(Phonon::MediaSource(fileName));
  dataStore->addMusicToLibrary(songList);
  syncLibrary();
}

void MetaWindow::setupUi(){

  playbackWidget = new PlaybackWidget(dataStore, this);

  libraryWidget = new LibraryWidget(dataStore, this);

  activityList = new ActivityList(dataStore);

  playlistView = new ActivePlaylistView(dataStore, this);

  QWidget* contentStackContainer = new QWidget(this);
  contentStack = new QStackedWidget(this);
  contentStack->addWidget(libraryWidget);
  contentStack->addWidget(playlistView);
  contentStack->setCurrentWidget(libraryWidget);
  QVBoxLayout *contentStackLayout = new QVBoxLayout;
  contentStackLayout->addWidget(contentStack, Qt::AlignCenter);
  contentStackContainer->setLayout(contentStackLayout);

  QSplitter *content = new QSplitter(Qt::Horizontal, this);
  content->addWidget(activityList);
  content->addWidget(contentStackContainer);
  content->setStretchFactor(1, 10);

  dashboard = new PlayerDashboard(dataStore, this);


  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->addWidget(dashboard);
  mainLayout->addWidget(content,6);
  mainLayout->addWidget(playbackWidget);

  QWidget* widget = new QWidget;
  widget->setLayout(mainLayout);

  setCentralWidget(widget);
  setWindowTitle("UDJ");

  connect(
    activityList,
    SIGNAL(libraryClicked()),
    this,
    SLOT(displayLibrary()));

  connect(
    activityList,
    SIGNAL(playlistClicked()),
    this,
    SLOT(displayPlaylist()));

  connect(
    libraryWidget,
    SIGNAL(libNeedsSync()),
    this,
    SLOT(syncLibrary()));
}

void MetaWindow::createActions(){
  quitAction = new QAction(tr("&Quit"), this);
  quitAction->setShortcuts(QKeySequence::Quit);
  addMusicAction = new QAction(tr("Add &Music"), this);
  addMusicAction->setShortcut(tr("Ctrl+M"));
  addSongAction = new QAction(tr("A&dd Song"), this);
  addSongAction->setShortcut(tr("Ctrl+D"));
  viewLogAction = new QAction(tr("View &Log"), this);
  viewLogAction->setShortcut(tr("Ctrl+L"));
  viewAboutAction = new QAction(tr("About"), this);
  connect(addMusicAction, SIGNAL(triggered()), this, SLOT(addMusicToLibrary()));
  connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
  connect(addSongAction, SIGNAL(triggered()), this, SLOT(addSongToLibrary()));
  connect(viewLogAction, SIGNAL(triggered()), this, SLOT(displayLogView()));
  connect(viewAboutAction, SIGNAL(triggered()), this, SLOT(displayAboutWidget()));
}

void MetaWindow::setupMenus(){
  QMenu *musicMenu = menuBar()->addMenu(tr("&Music"));
  musicMenu->addAction(addMusicAction);
  musicMenu->addAction(addSongAction);
  musicMenu->addSeparator();
  musicMenu->addAction(quitAction);

  configurePlayerMenu();

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
  helpMenu->addAction(viewLogAction);
  helpMenu->addAction(viewAboutAction);

}

void MetaWindow::configurePlayerMenu(){
  QMenu *playerMenu = menuBar()->addMenu(tr("&Player"));

  changeNameAction = new QAction(tr("Change Name"), this);
  playerMenu->addAction(changeNameAction);

  setLocationAction = new QAction(tr("Set Location"), this);
  playerMenu->addAction(setLocationAction);

  setPasswordAction = new QAction(tr("Set Password"), this);
  playerMenu->addAction(setPasswordAction);

  removePasswordAction = new QAction(tr("Remove Password"), this);
  playerMenu->addAction(removePasswordAction);
  removePasswordAction->setEnabled(dataStore->hasPlayerPassword());

  connect(changeNameAction, SIGNAL(triggered()), this, SLOT(changePlayerName()));
  connect(setLocationAction, SIGNAL(triggered()), this, SLOT(setPlayerLocation()));
  connect(setPasswordAction, SIGNAL(triggered()), this, SLOT(setPlayerPassword()));
  connect(removePasswordAction, SIGNAL(triggered()), this, SLOT(removePlayerPassword()));
}

void MetaWindow::removePlayerPassword(){
  QMessageBox::StandardButton response = QMessageBox::question(
      this, tr("Remove password"), tr("Are you sure you want to remove the player password"),
      QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
  if(response == QMessageBox::Ok){
    connect(
      dataStore,
      SIGNAL(playerPasswordRemoved()),
      this,
      SLOT(onPlayerPasswordRemoved()));
    connect(
      dataStore,
      SIGNAL(playerPasswordRemoveError(const QString&)),
      this,
      SLOT(onPlayerPasswordRemoveError()));
    dataStore->removePlayerPassword();
  }
}

void MetaWindow::disconnectPlayerPasswordRemoveSignals(){
    disconnect(
      dataStore,
      SIGNAL(playerPasswordRemoved()),
      this,
      SLOT(onPlayerPasswordRemoved()));
    disconnect(
      dataStore,
      SIGNAL(playerPasswordRemoveError(const QString&)),
      this,
      SLOT(onPlayerPasswordRemoveError()));
}


void MetaWindow::onPlayerPasswordRemoved(){
  disconnectPlayerPasswordRemoveSignals();
}

void MetaWindow::onPlayerPasswordRemoveError(){
  disconnectPlayerPasswordRemoveSignals();
  QMessageBox::critical(this, tr("Error Removing Password"), tr("Oops. We couldn't remove the "
      "player's password. We're super sorry. Can you try it again in a little bit?"));
}

void MetaWindow::setPlayerPassword(){
  SetPasswordDialog *setPasswordDialog = new SetPasswordDialog(dataStore, this);
  setPasswordDialog->show();
}

void MetaWindow::setPlayerLocation(){
  SetLocationDialog *setLocationDialog = new SetLocationDialog(dataStore, this);
  setLocationDialog->show();
}

void MetaWindow::changePlayerName(){
  bool gotNewName;
  QString newName = QInputDialog::getText(this, tr("Set Player Name"),
    tr("New Player Name:"), QLineEdit::Normal, "", &gotNewName);
  if(gotNewName && !newName.isEmpty()){
    connect(
      dataStore,
      SIGNAL(playerNameChanged(const QString&)),
      this,
      SLOT(onPlayerNameChanged()));
    connect(
      dataStore,
      SIGNAL(playerNameChangeError(const QString&)),
      this,
      SLOT(onPlayerNameChangeError(const QString&)));
    dataStore->setPlayerName(newName);
  }
  else if(gotNewName){
    QMessageBox::critical(this, "Player Name Blank", "You must provided a non-blank name for your player");
  }

}

void MetaWindow::disconnectNameChangeSignals(){
  disconnect(
      dataStore,
      SIGNAL(playerNameChanged(const QString&)),
      this,
      SLOT(onPlayerNameChanged()));
  disconnect(
      dataStore,
      SIGNAL(playerNameChangeError(const QString&)),
      this, SLOT(onPlayerNameChangeError(const QString&)));
}

void MetaWindow::onPlayerNameChanged(){
  disconnectNameChangeSignals();
}

void MetaWindow::onPlayerNameChangeError(const QString& errMessage){
  disconnectNameChangeSignals();
  QMessageBox::critical(this, "Error Changing Player Name", errMessage);
}


void MetaWindow::displayLibrary(){
  contentStack->setCurrentWidget(libraryWidget);
}

void MetaWindow::displayPlaylist(){
  contentStack->setCurrentWidget(playlistView);
}

void MetaWindow::syncLibrary(){
  syncingProgress = new QProgressDialog(
    "Syncing Library...", "Cancel", 0, dataStore->getTotalUnsynced(), this);
  syncingProgress->setWindowModality(Qt::WindowModal);
  syncingProgress->setMinimumDuration(250);
  syncingProgress->setCancelButton(0);
  connect(
    dataStore,
    SIGNAL(libSongsModified(const QSet<library_song_id_t>&)),
    this,
    SLOT(syncUpdate(const QSet<library_song_id_t>&)));
  connect(
    dataStore,
    SIGNAL(allSynced()),
    this,
    SLOT(syncDone()));
  connect(
    dataStore,
    SIGNAL(libModError(const QString&)),
    this,
    SLOT(syncError(const QString&)));
  dataStore->syncLibrary();
  syncingProgress->setValue(0);
}

void MetaWindow::syncUpdate(const QSet<library_song_id_t>& songs){
  syncingProgress->setValue(syncingProgress->value() + songs.size());
}

void MetaWindow::disconnectSyncSignals(){
  disconnect(
    dataStore,
    SIGNAL(libSongsModified(const QSet<library_song_id_t>&)),
    this,
    SLOT(syncUpdate(const QSet<library_song_id_t>&)));
  disconnect(
    dataStore,
    SIGNAL(allSynced()),
    this,
    SLOT(syncDone()));
  disconnect(
    dataStore,
    SIGNAL(libModError(const QString&)),
    this,
    SLOT(syncError(const QString&)));
}

void MetaWindow::syncDone(){
  disconnectSyncSignals();
  syncingProgress->close();
}

void MetaWindow::syncError(const QString& /*errMessage*/){
  disconnectSyncSignals();
  syncingProgress->close();
  QMessageBox::critical(this, "Error", "Error syncing library. We'll try again next time you startup UDJ");
}

void MetaWindow::displayLogView(){
  LogViewer *viewer = new LogViewer();
  viewer->show();
}

void MetaWindow::displayAboutWidget(){
  AboutWidget *about = new AboutWidget();
  about->show();
}

void MetaWindow::enableRemovePassword(){
  removePasswordAction->setEnabled(true);
}

void MetaWindow::disableRemovePassword(){
  removePasswordAction->setEnabled(false);
}

} //end namespace
