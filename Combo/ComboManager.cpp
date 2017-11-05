/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of combo manager class
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "ComboManager.h"
#include "InputManager.h"
#include "BeeftextGlobals.h"
#include <XMiLib/Exception.h>


namespace {
   QString const kComboListFileName = "comboList.json"; ///< The name of the default combo list file
}

//**********************************************************************************************************************
/// \return A reference to the only allowed instance of the class
//**********************************************************************************************************************
ComboManager& ComboManager::instance()
{
   static ComboManager instance;
   return instance;
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
ComboManager::~ComboManager()
{
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
ComboManager::ComboManager()
   : QObject()
{
   InputManager& inputManager = InputManager::instance();
   connect(&inputManager, &InputManager::comboBreakerTyped, this, &ComboManager::onComboBreakerTyped);
   connect(&inputManager, &InputManager::characterTyped, this, &ComboManager::onCharacterTyped);
   connect(&inputManager, &InputManager::backspaceTyped, this, &ComboManager::onBackspaceTyped);

   QFile file(QDir(globals::getAppDataDir()).absoluteFilePath(kComboListFileName));
   if (!file.open(QIODevice::ReadOnly))
      globals::debugLog().addWarning("No combo list file could be found.");
   else
   {
      QString errMsg;
      if (!comboList_.readFromJsonDocument(QJsonDocument::fromJson(file.readAll()), &errMsg))
         globals::debugLog().addError("The combo list file is invalid.");
   }
   
   /// \todo remove this debug code
   QString errMsg;
   if (!this->saveComboListToFile(&errMsg))
      QMessageBox::critical(nullptr, tr("Error"), errMsg);
}


//**********************************************************************************************************************
/// \param[out] outErrorMessage If not null the function returns false, this variable will contain a description 
/// of the error.
/// \return true if and only if the operation completed successfully
//**********************************************************************************************************************
bool ComboManager::saveComboListToFile(QString* outErrorMsg)
{
   try
   {
      QFile file(QDir(globals::getAppDataDir()).absoluteFilePath(kComboListFileName));
      if (!file.open(QIODevice::WriteOnly))
         throw xmilib::Exception("The combo list could not be saved.");
      file.write(comboList_.toJsonDocument().toJson());
      return true;
   }
   catch (xmilib::Exception const& e)
   {
      if (outErrorMsg)
         *outErrorMsg = e.qwhat();
      return false;
   }
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void ComboManager::onComboBreakerTyped()
{
   currentText_ = QString();
}


//**********************************************************************************************************************
/// \param[in] c The character that was typed
//**********************************************************************************************************************
void ComboManager::onCharacterTyped(QChar c)
{
   currentText_.append(c);
   globals::debugLog().addInfo(QString("Character %1 was type. Combo text is now %2").arg(c).arg(currentText_));
   VecSPCombo::const_iterator it = std::find_if(comboList_.begin(), comboList_.end(),
      [&](SPCombo const combo) -> bool { return combo->comboText() == currentText_; });
   if (comboList_.end() == it)
      return;
   (*it)->performSubstitution();
   this->onComboBreakerTyped();
}


//**********************************************************************************************************************
// 
//**********************************************************************************************************************
void ComboManager::onBackspaceTyped()
{
   currentText_.chop(1);
}