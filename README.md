# sms_w2a
Helps you transfer your SMS messages from **Windows** phone to **Android** phone, and vice versa
Importing Nokia Suite messages is supported as well

## Tools
You'll have to use specialized (free!) apps on each platform to backup and restore your messages.
* **Windows** phone: **[contacts+message backup](https://www.microsoft.com/en-us/store/p/contacts-message-backup/9nblgggz57gm)**, by Microsoft Corporation
* **Android** phone: **[SMS Backup & Restore](https://play.google.com/store/apps/details?id=com.riteshsahu.SMSBackupRestore)**, by Carbonite
* **Symbian** phone: **[Nokia Suite](https://en.wikipedia.org/wiki/Nokia_Suite)**, by Nokia
* **[sms_w2a.exe](../../releases/latest)** will make the conversion from one format to the other

## Features
* Convert SMS messages from Windows <-> Android, both ways
* Import SMS messages from Nokia Suite
* Full support for Unicode characters such as emoji
* Supports messages sent to multiple recipients
* Offline conversion. Your messages won't leave your computer, no clouds involved

## Credits
* Credit goes to @github/gpailler for his wonderful **contacts+message backup** hash reverse engineering. Check out his [Android2Wp_SMSConverter](https://github.com/gpailler/Android2Wp_SMSConverter) project as well!
* **sms_w2a** is using a customized version of [RapidXML](http://rapidxml.sourceforge.net)
* **sms_w2a** is using [libcsv](https://github.com/rgamble/libcsv)
