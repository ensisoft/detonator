# DETONATOR 2D

## Known Issues 🤬

List of known issues that are not clearly identified as DETONATOR 2D bugs (yet).

### Linux

Pulseaudio  audio issue that happens occasionally. Pulseaudio sound is crackling a lot and several buffer 
underruns are reported during the stream playback. Increasing audio buffer size or the audio device poll frequency makes no difference.<br>
Audio test cases might hang and never finish.

Issue started happening after some distro level system update.<br> 
Pulseaudio version 16.1

Current workaround is to try to restart pulseaudio.
```
$ pulseaudio -k
$ pulseaudio --start
```

### Windows

### HTML5/WASM


## Qt5 Quirks and bugggggsss 🤬

### !!!!  NEVER TRUST THE DESIGNER BUT ALWAYS CHECK THE XML!!!1 !!!


#### QPushButton borks with 'auto default'

- Buttons when added to a dialog can have 'auto default' property which creates surprising 
behaviour when 'enter' is pressed. The button will capture the 'enter' and accept the dialog.
* Designer often borks and fails to show the auto default properly. DON'T TRUST THE DESIGNER!
* Sometimes bug happens when there's *no* autoDefault property in the .xml. Make sure to *ADD* autoDefault with false.
* Check the generated .xml for the auto default for the real value. 

```
     <widget class="QPushButton" name="btnCancel">
       <property name="text">
        <string>Cancel</string>
       </property>
       <property name="autoDefault">
        <bool>false</bool>
       </property>
      </widget>
```   

#### QComboBox creates layout issues with its size adjust policy
- This is about the way the QComboBox absolutely wants to resize itself according to it's content.
Intention is probably all good but in practice this will often cause weird layout issues and mis-alignment.
- There's no "don't adjust contents" policy, but looks like using "fixed" size policy helps?

#### QDialog exec() no longer modal after show()

```
void MainWindow::something()
{
    DlgFoobar dlg(this);
    dlg.show();
    ...
    dlg.exec();
}
```

Calling show before exec makes the dialog lose modality. 
In other words the user can click on other controls in the mainwindow.

Comments on exec()

So calling exec() without show() being called first works as expected. The dialog remains modal and the user is restricted to only applying input on the dialog.

The problem is that using exec() is not adequate.

Consider a case where you wish to restore the dialog state to a previous state, i.e. populate the dialog and its data in some meaningful way.

To do this with smooth UX you really need to:

1. Load and restore the dialog geometry *before* showing the dialog.<br>
   If you first show and then restore the resizing will cause ugly visual flicker and disturbance.
2. Show it after the geometry has been restored
3. Then load any other state, including state that might fail (for example imagine a file is no longer available)<br>
3.1 You need to have the dialog open so that you can open a QMessageBox properly using the dialog as the window parent in order to open the msg box properly.<br> 
3.2 Finally call exec() and block the user to the dialog.


#### QScrollAreaWidget simply HATES YOU!

- https://www.youtube.com/watch?v=8xvZ3Qj1KVk
