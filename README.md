<p align="center">
  <img src="images/shoji-logo.jpg" width="360" />
</p>
<h1 align="center">Shoji - a dynamic, template-based Entity Editor</h1>

<p align="center"><em>a universal, customizable, template-driven and metadata-centric entity viewer and editor for Haiku.</em></p>

# About

"Shoji" is the Japanese word for the typical sliding doors with paper fillings and wooden frames.
The kanji used in the logo have individual meanings that reflect the essence and features of this project:

* 型 (kata) - meaning "form," "type," or "template"
* 構 (kou) - meaning "structure" or "framework"
* 編 (hen) - meaning "edit" or "compile"
* 文 (bun/mon) - meaning "document" or "text"

The shoji metaphor captures both the transparency (you can see your data) and the structural flexibility (dynamic sliding doors to reconfigure views based on Entity types and user needs).

Together, 編集 (henshū) actually forms a compound word meaning "editing" or "compilation" - which captures both the individual manipulation of files (編) and their organized collection/management (集).

This pairing expresses the core functionality of this SEN Labs project: the ability to edit and organize files as structured objects.

# Using it

`shoji <file>` (or Open With... Shoji) opens an entity: a thumbnail of the file (its `Media:Thumbnail`, else its icon) on the dark stripe and the attributes of its MIME type on the light panel.

* Only attributes that the MIME type declares **viewable** in its attribute info are shown, and only those declared **editable** can be changed, if the file itself is writable.
* The view is a template for the MIME type (an archived `BView` in `~/config/settings/shoji/templates/<type>/template.view`, controls named after the attributes), or, without one, a generated form with a control for the type of each attribute.
* Changes are written to the attributes of the file as soon as a field is left (click elsewhere, Tab, Enter, another window, closing); a check box when clicked. A value that does not fit the type is refused and the field goes back. An empty value removes the attribute.
* An attribute that the MIME type declares a **list** (`SEN:is_list` and the separator `SEN:list_separator` in its attribute info, written by the ontologies: authors, subjects, the groups of a person) is shown as a list of values with Add and Remove, and the value that is selected is edited in a field below it. The values are kept in one string, separated as the type says (a comma by default; a value that contains it is in quotes: `"Hunt, Andy",Dave Thomas`). A template that has a text field for such an attribute gets the list instead. A list that you did not change is not rewritten.
* The form is centred on the line through the middle of the picture (a third of the height of the window), for every view, and the window is never smaller than its form needs.
* **Revert** puts all attributes back as the file had them when it was opened (or when someone else last changed them).
* The file is watched: when another program changes an attribute that you are not editing, the field shows it at once; when it changes one that you have edited and not stored, Shoji asks whether to keep your change or take the new value.

Supported types: strings, booleans, integers (8 to 64 bit, signed and unsigned), float, double and time (`YYYY-MM-DD HH:MM:SS`). Other types are shown as read-only.

Templates that come with Shoji: `application/x-person` (a contact) and `entity/x-vnd.sen-labs.entity.book` (a book: title, authors, ISBN, pages, publisher, date, language and subjects; the cover is the
thumbnail on the stripe, e.g. from the book enricher BERT of SENSEI). `./build.sh` generates them (`bin/shoji -g`) into `~/config/settings/shoji/templates/<MIME type>/template.view`; the package (`./package.sh`) puts them into `data/shoji/templates`. Shoji looks in this order and takes the first template: the settings of the user (so a template of your own replaces one that a package brings), the data folders of the user, the system's data folder (packages), and the non-packaged data folders. A template is a view whose controls are
named after the attributes; a new one is a class in `src/templates/<MIME type>/` and an entry in the table in `ShojiApp.cpp`.

Building needs the SEN headers (`sento/install.sh`) for the stripe of the About windows (`<sen/SenStripeDialog.h>`).

