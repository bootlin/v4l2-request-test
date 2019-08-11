<!-- v4l2-request-test README.md -->
<!-- version: 0.0.1 -->

# v4l2-request-test

<p align="center">
  <span>English</span> |
  <a href="lang/french#v4l2-request-test">Français</a> | -->
  <a href="lang/german#v4l2-request-test">Deutsch</a>
</p>

## Über

`v4l2-request-test` wurde als eigenständiges Tool entwickelt, das bei der Entwickung
und dem Test des Video Treibers `Cedrus` zum Einsatz kommt.
Das Tool unterstützt die Video Engine, wie sie in den meisten Allwinner SOCs verbaut ist.
Es implementiert die Linux [V4L2 Request API](https://www.linuxtv.org/downloads/v4l-dvb-apis-new/uapi/mediactl/request-api.html).
Dadurch kann es eingesetzt werden, um auch andere Treiber zu testen, die auf diesem API aufbauen.

Derzeit unterstützt `v4l2-request-test` die Decodierung von MPEG-2 und H264 Daten-Frames. Diese sind
als Dateien (slice data), inclusive zugehöriger Metadaten gespeichert. Die Dateien werden decodiert
und anschließend auf einem dedizierten DRM Plane ausgegeben.

Das Verhalten des Tools kann über Komandozeilen Argumente konfiguriert werden. Der Hilfetext
beschreibt mögliche Werte.

## Wie es funktioniert

TODO: summarize the functional structure of the code.

## Presets

### Preset Definitionen

Um den Test der unterstützen Codecs zu erleichtern definiert `v4l2-request-test` eine sogenannte
`Preset` Struktur. Diese Struktur ordnet ihre Attribute als Deklarative-, bzw. als Kontroll-Felder.

* declarative fields

Deklarative Strukturelemente beschreiben die Herkunft des kodierten Frame-Sets.

  * name
  * description
  * license
  * attribution
  * control fields

Jedes `Preset` speichert Attribute, die der Natur des zu dekodierenden Frames entsprechen. Dies
Informationen werden für die Verarbeitung mit dem korrekten Algorithmus herangezogen.

  * codec type
  * buffer_count,
  * frames
  * frames_count
  * data path

Eine Serie kodierter Frames wurden als Dateien im `data` Unterverzeichnes gespeichert. Die Namen
der Unterverzeichisse entsprechen  dem Preset `name` Attribut.

### Preset Daten

Es existieren Unterverzeichnisse mit codierten Frames für die folgenden `Presets`:

* bbb-mpeg2
* bbb-happy-mpeg2
* ed-mpeg2
* bbb-h264-32
* bbb-h264-all-i-32
* bbb-h264-high-32
* caminandes-fall-h265
* caminandes-h265

## Mitarbeit

Hilfe ist sehr willkommen! Gerne könnt Ihr das Projekt forken und PR's einreichen,
um neue Funktion zu implementieren oder Fehler zu bereinigen.

## Lizenz

<!-- License source -->
[Logo-CC_BY]: https://i.creativecommons.org/l/by/4.0/88x31.png "Creative Common Logo"
[License-CC_BY]: https://creativecommons.org/licenses/by/4.0/legalcode "Creative Common License"

Diese Arbeit ist unter der [Creative Common License 4.0][License-CC_BY] lizensiert.

![Creative Common Logo][Logo-CC_BY]

© 2017 - 2019 Paul Kocialkowski (bootlin);
© 2019        Ralf Zerres (networkx)
