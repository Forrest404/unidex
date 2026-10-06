# Home tiles are shot after visiting the app, so their lines come from the sample data too.
# Screens for the README strip (docs/screens.png) and the link preview (site/og.png). Timetable and Notes show the
# test build's sample classes and notes (demo 1), so no one's real timetable or notes appear; no Dex list (nearby
# network names), no badges (photos). Chooser's spin adds one win to its tally (normal use); Dino's score isn't
# saved (dry run).
dry 1
demo 1
select Timetable
press b
shot r-timetable
press b
shot r-class
press A
press B
shot r-today
press A
press A
expect screen=Home
shot r-home-timetable
select Notes
press b
shot r-notes-main
press a
shot r-notes-list
press b
shot r-note
press A
press A
press A
expect screen=Home
shot r-home-notes
select Chooser
press b
press b
shot r-chooser-result
press A
press A
expect screen=Home
seed 11
manual 1
select Games
shot r-home-games
press b
pick Dino
press b
play 10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
play 000000000001111111111000000
play 00000000000000000000000000000000000000000000000000000000000000000000000000000000000011111111110000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
play 0000111111
shot r-dino
manual 0
press A
press A
press A
expect screen=Home
