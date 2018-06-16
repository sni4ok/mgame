" usefull commands for vim users, 
" :F smth for search smth in current cpp project
" :FF - all cpp projects
" for apply this commands exrc should be set in ~/.vimrc (":set exrc") and vim should be started from current directory

:command -nargs=1 F vimgrep /<args>/j *.hpp *.cpp|:cw
:command -nargs=1 FF vimgrep /<args>/j ../evie/*.?pp ../makoa/*.?pp ../tyra/*.?pp ../viktor/*.?pp ../ying/*.?pp ../alco/*/*.?pp|:cw


