/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of preferences dialog
///
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "PreferencesDialog.h"
#include "ShortcutDialog.h"
#include "Combo/ComboManager.h"
#include "InputManager.h"
#include "I18nManager.h"
#include "Backup/BackupManager.h"
#include "Backup/BackupRestoreDialog.h"
#include "UpdateManager.h"
#include "BeeftextUtils.h"
#include "BeeftextGlobals.h"
#include "BeeftextConstants.h"


namespace {
qint32 kUpdateCheckStatusLabelTimeoutMs = 3000; ///< The delay after which the update check status label is cleared
}


//**********************************************************************************************************************
/// \param[in] parent The parent widget of the dialog
//**********************************************************************************************************************
PreferencesDialog::PreferencesDialog(QWidget* parent)
   : QDialog(parent, constants::kDefaultDialogFlags)
   , prefs_(PreferencesManager::instance())
{
   ui_.setupUi(this);
   this->updateCheckStatusTimer_.setSingleShot(true);
   connect(&updateCheckStatusTimer_, &QTimer::timeout, [&]() { ui_.labelUpdateCheckStatus->setText(QString()); });
   ui_.labelUpdateCheckStatus->setText(QString());
   ui_.checkAutoStart->setText(tr("&Automatically start %1 at login").arg(constants::kApplicationName));
   this->loadPreferences();
   this->applyAutoStartPreference(); // we ensure autostart is properly setup
   this->applyThemePreference(); // we apply the custom theme if the user selected it
   I18nManager::fillLocaleCombo(*ui_.comboLocale);
   I18nManager::selectLocaleInCombo(I18nManager::instance().locale(), *ui_.comboLocale);
   if (isInPortableMode())
   {
      QWidgetList widgets = { ui_.checkAutoStart, ui_.frameComboListFolder };
      for (QWidget* widget : widgets)
         widget->setVisible(false);
   }

   // signal mappings for the 'Check now' button
   UpdateManager& updateManager = UpdateManager::instance();
   connect(ui_.buttonCheckNow, &QPushButton::clicked, &updateManager, &UpdateManager::checkForUpdate);
   connect(&updateManager, &UpdateManager::startedUpdateCheck, [&]() { ui_.buttonCheckNow->setEnabled(false); });
   connect(&updateManager, &UpdateManager::finishedUpdateCheck, [&]() { ui_.buttonCheckNow->setEnabled(true); });
   connect(&updateManager, &UpdateManager::updateIsAvailable, this, &PreferencesDialog::onUpdateIsAvailable);
   connect(&updateManager, &UpdateManager::noUpdateIsAvailable, this, &PreferencesDialog::onNoUpdateIsAvailable);
   connect(&updateManager, &UpdateManager::updateCheckFailed, this, &PreferencesDialog::onUpdateCheckFailed);

   // We update the GUI when the combo list is saved to proper enable/disable the 'Restore Backup' button
   connect(&ComboManager::instance(), &ComboManager::comboListWasSaved, this, &PreferencesDialog::updateGui);

   this->updateGui();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::loadPreferences() const
{
   QSignalBlocker blockers[] = { QSignalBlocker(ui_.checkPlaySoundOnCombo),
      QSignalBlocker(ui_.checkAutoCheckForUpdates), QSignalBlocker(ui_.checkUseClipboardForComboSubstitution),
      QSignalBlocker(ui_.checkAutoStart), QSignalBlocker(ui_.radioComboTriggerAuto),
      QSignalBlocker(ui_.radioComboTriggerManual), QSignalBlocker(ui_.checkAutoBackup) }; // Temporarily Block signals emission by the controls
   ui_.checkPlaySoundOnCombo->setChecked(prefs_.playSoundOnCombo());
   ui_.checkAutoCheckForUpdates->setChecked(prefs_.autoCheckForUpdates());
   ui_.checkUseClipboardForComboSubstitution->setChecked(prefs_.useClipboardForComboSubstitution());
   ui_.checkAutoStart->setChecked(prefs_.autoStartAtLogin());
   ui_.checkUseCustomTheme->setChecked(prefs_.useCustomTheme());
   if (prefs_.useAutomaticSubstitution())
      ui_.radioComboTriggerAuto->setChecked(true);
   else
      ui_.radioComboTriggerManual->setChecked(true);
   SPShortcut const shortcut = InputManager::instance().comboTriggerShortcut();
   ui_.editShortcut->setText(shortcut ? shortcut->toString() : "");
   ui_.editComboListFolder->setText(QDir::toNativeSeparators(prefs_.comboListFolderPath()));
   ui_.checkAutoBackup->setChecked(prefs_.autoBackup());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::applyAutoStartPreference() const
{
   if (prefs_.autoStartAtLogin())
   {
      if (!registerApplicationForAutoStart())
         globals::debugLog().addWarning("Could not register the application for automatic startup on login.");
   }
   else
      unregisterApplicationFromAutoStart();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::applyThemePreference() const
{
   qApp->setStyleSheet(prefs_.useCustomTheme() ? constants::kStyleSheet : QString());
}


//**********************************************************************************************************************
/// \param[in] folderPath The new path of the folder
/// \param[in] previousPath The path of the previous folder, to restore in case the new one does not work
//**********************************************************************************************************************
void PreferencesDialog::applyComboListFolderPreference(QString const& folderPath, QString const& previousPath)
{
   if (isInPortableMode())
      return;
   PreferencesManager& prefs = PreferencesManager::instance();
   prefs.setComboListFolderPath(folderPath);
   QString err;
   if (!ComboManager::instance().saveComboListToFile(&err))
   {
      prefs.setComboListFolderPath(previousPath);
      globals::debugLog().addError(QString("The combo list folder could not be changed to '%1'").arg(folderPath));
      QMessageBox::critical(this, tr("Error"), tr("The combo list folder could not be changed."));
      return;
   }
   ui_.editComboListFolder->setText(QDir::toNativeSeparators(folderPath));
}



//**********************************************************************************************************************
/// \param[in] status The status message
//**********************************************************************************************************************
void PreferencesDialog::setUpdateCheckStatus(QString const& status)
{
   updateCheckStatusTimer_.stop();
   ui_.labelUpdateCheckStatus->setText(status);
   updateCheckStatusTimer_.start(kUpdateCheckStatusLabelTimeoutMs);
}


//**********************************************************************************************************************
/// \param[in] event The event
//**********************************************************************************************************************
void PreferencesDialog::changeEvent(QEvent *event)
{
   if (QEvent::LanguageChange == event->type())
   {
      ui_.retranslateUi(this);
      SPShortcut const shortcut = InputManager::instance().comboTriggerShortcut();
      ui_.editShortcut->setText(shortcut ? shortcut->toString() : "");
   }
   QDialog::changeEvent(event);
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::updateGui() const
{
   bool const manualTrigger = !prefs_.useAutomaticSubstitution();
   ui_.editShortcut->setEnabled(manualTrigger);
   ui_.buttonChangeShortcut->setEnabled(manualTrigger);
   ui_.buttonResetComboTriggerShortcut->setEnabled(manualTrigger);
   ui_.buttonRestoreBackup->setEnabled(prefs_.autoBackup() && BackupManager::instance().backupFileCount());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionResetToDefaultValues()
{
   if (QMessageBox::Yes != QMessageBox::question(this, tr("Reset Preferences"), tr("Are you sure you want to reset "
      "the preferences to their default values?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
      return;
   PreferencesManager& prefs = PreferencesManager::instance();
   QString const oldComboListFolderPath = prefs.comboListFolderPath();
   prefs_.reset();
   InputManager::instance().setComboTriggerShortcut(prefs_.comboTriggerShortcut());
   this->loadPreferences();
   this->applyAutoStartPreference();
   this->applyThemePreference();
   this->applyComboListFolderPreference(prefs.comboListFolderPath(), oldComboListFolderPath);
   InputManager::instance().setComboTriggerShortcut(PreferencesManager::defaultComboTriggerShortcut());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionChangeComboListFolder()
{
   QString const path = QFileDialog::getExistingDirectory(this, tr("Select folder"),
      QDir::fromNativeSeparators(ui_.editComboListFolder->text()));
   if (path.trimmed().isEmpty())
      return;
   this->applyComboListFolderPreference(path, PreferencesManager::instance().comboListFolderPath());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionResetComboListFolder()
{
   PreferencesManager& prefs = PreferencesManager::instance();
   this->applyComboListFolderPreference(PreferencesManager::defaultComboListFolderPath(), prefs.comboListFolderPath());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionChangeShortcut() const
{
   ShortcutDialog dlg(InputManager::instance().comboTriggerShortcut());
   if (QDialog::Accepted != dlg.exec())
      return;
   SPShortcut const shortcut = dlg.shortcut();
   ui_.editShortcut->setText(shortcut->toString());
   InputManager::instance().setComboTriggerShortcut(shortcut);
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionResetComboTriggerShortcut() const
{
   SPShortcut const shortcut = PreferencesManager::defaultComboTriggerShortcut();
   InputManager::instance().setComboTriggerShortcut(shortcut);
   ui_.editShortcut->setText(shortcut ? shortcut->toString() : "");
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionRestoreBackup()
{
   BackupRestoreDialog::run(this);
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onPlaySoundOnComboCheckChanged() const
{
   prefs_.setPlaySoundOnCombo(ui_.checkPlaySoundOnCombo->isChecked());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onAutoStartCheckChanged() const
{
   if (isInPortableMode())
      return;
   prefs_.setAutoStartAtLogin(ui_.checkAutoStart->isChecked());
   this->applyAutoStartPreference();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onAutoCheckForUpdatesCheckChanged() const
{
   prefs_.setAutoCheckForUpdates(ui_.checkAutoCheckForUpdates->isChecked());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onUseCustomThemeCheckChanged() const
{
   prefs_.setUseCustomTheme(ui_.checkUseCustomTheme->isChecked());
   this->applyThemePreference();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onRadioAutomaticComboTriggerChecked(bool checked) const
{
   prefs_.setUseAutomaticSubstitution(ui_.radioComboTriggerAuto->isChecked());
   this->updateGui();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onUseClipboardForComboSubstitutionCheckChanged() const
{
   prefs_.setUseClipboardForComboSubstitution(ui_.checkUseClipboardForComboSubstitution->isChecked());
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onAutoBackupCheckChanged()
{
   BackupManager& backupManager = BackupManager::instance();
   bool const isChecked = ui_.checkAutoBackup->isChecked();
   if (isChecked || (0 == backupManager.backupFileCount()))
   {
      prefs_.setAutoBackup(isChecked);
      this->updateGui();
      return;
   }

   switch (QMessageBox::question(this, tr("Delete Backup Files?"), tr("Do you want to delete all the "
      "backup files?"), QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel),
      QMessageBox::No))
   {
   case QMessageBox::Yes:
      backupManager.removeAllBackups();
      // no break on purpose
   case QMessageBox::No:
      prefs_.setAutoBackup(false);
      break;
   case QMessageBox::Cancel:
   default:
      // Changing the state of the check box inside this slot cause the next event to be missed so we delay it using a timer
      QTimer::singleShot(0,
         [&]() { QSignalBlocker blocker(ui_.checkAutoBackup); ui_.checkAutoBackup->setChecked(true); });
      break;
   }
   this->updateGui();
}


//**********************************************************************************************************************
//
//**********************************************************************************************************************
void PreferencesDialog::onLocaleChanged() const
{
   QLocale const locale = I18nManager::getSelectedLocaleInCombo(*ui_.comboLocale);
   prefs_.setLocale(locale);
   I18nManager::instance().setLocale(locale);
}


//**********************************************************************************************************************
/// \param[in] latestVersionInfo The latest version information
//**********************************************************************************************************************
void PreferencesDialog::onUpdateIsAvailable(SPLatestVersionInfo const& latestVersionInfo)
{
   this->setUpdateCheckStatus(latestVersionInfo ? tr("%1 v%2.%3 is available.").arg(constants::kApplicationName)
      .arg(latestVersionInfo->versionMajor()).arg(latestVersionInfo->versionMinor())
      : tr("A new version is available."));
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onNoUpdateIsAvailable()
{
   this->setUpdateCheckStatus(tr("The software is up to date."));
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onUpdateCheckFailed()
{
   this->setUpdateCheckStatus(tr("Update check failed."));
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void PreferencesDialog::onActionOk()
{
   this->accept();
}
