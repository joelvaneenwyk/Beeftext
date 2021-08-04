﻿/// \file
/// \author 
///
/// \brief Implementation of snippet fragment class.
///  
/// Copyright (c) . All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information. 


#include "stdafx.h"
#include "SnippetFragment.h"
#include "TextSnippetFragment.h"
#include "DelaySnippetFragment.h"
#include "KeySnippetFragment.h"


namespace {


qint32 constexpr kDelayBetweenFragmentsMs = 100; ///< THe delay between fragments in milliseconds


}


//**********************************************************************************************************************
/// \brief split a string into snippet fragment at the #{key:} variable.
/// \param[in] str The string.
/// \return the text split into fragments
//**********************************************************************************************************************
ListSpSnippetFragment splitForKeyVariable(QString const& str)
{
   ListSpSnippetFragment result;
   QString s(str);
   QRegularExpression const rx(R"(^(.*)#{key:(\w+)}(.*)$)", QRegularExpression::InvertedGreedinessOption);
   QRegularExpressionMatch match;
   while ((match = rx.match(s)).hasMatch())
   {
      QString const before = match.captured(1);
      if (!before.isEmpty())
         result.append(std::make_shared<TextSnippetFragment>(before));
      result.append(std::make_shared<KeySnippetFragment>(match.captured(2)));
      s = match.captured(3);
   }
   if (!s.isEmpty())
      result.append(std::make_shared<TextSnippetFragment>(s));
   return result;
}


//**********************************************************************************************************************
/// \param[in] str The string to split.
/// \return the list of fragments
//**********************************************************************************************************************
ListSpSnippetFragment splitStringIntoSnippetFragments(QString const& str)
{
   ListSpSnippetFragment result;
   QString s(str);
   QRegularExpression const rx(R"(^(.*)#{delay:(\d+)}(.*)$)", QRegularExpression::InvertedGreedinessOption);
   QRegularExpressionMatch match;
   while ((match = rx.match(s)).hasMatch())
   {
      QString const before = match.captured(1);
      if (!before.isEmpty())
         result.append(splitForKeyVariable(before));
      bool ok = false;
      qint32 const delay = match.captured(2).toInt(&ok);
      if (ok && (delay > 0))
         result.append(std::make_shared<DelaySnippetFragment>(delay));

      s = match.captured(3);
   }
   if (!s.isEmpty())
      result.append(splitForKeyVariable(s));
   return result;
}


//**********************************************************************************************************************
/// \param[in] fragments The list of snippet fragments.
///
/// \note this function does not disable the keyboard hook before operating.
//**********************************************************************************************************************
void renderSnippetFragmentList(ListSpSnippetFragment const& fragments)
{
   qDebug() << "--- START ---";
   qsizetype const count = fragments.size();
   for (qsizetype i = 0; i < fragments.size(); ++i)
   {
      SpSnippetFragment const fragment = fragments[i];
      if (!fragment)
         continue;
      fragment->render();
      qDebug() << fragment->toString();
      if (i != count - 1)
         QThread::msleep(kDelayBetweenFragmentsMs);
   }
   qDebug() << "---  END  ---";
}


