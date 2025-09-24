#!/usr/bin/env bash

GIT_ARGS="--no-pager"

readonly GREEN='\033[0;32m'
readonly YELLOW='\033[0;33m'
readonly RED='\033[0;31m'
readonly RESET='\033[0m'

# check that we do not use tabs
if git $GIT_ARGS grep -n --column \
    -e $'\t' --and --not -e "IGNORE_TAB"  \
    -- "*.[ch]" \
       "*[ch]pp" \
       "*.sh" \
       "*.json" \
       ":(exclude,top)cpp/ext/*"
 then
    echo -e ""
    echo -e "${YELLOW}Tabs were found in the files above!"
    echo -e "Please replace each tab with two spaces.${RESET}"
    echo -e ""
    echo -e "${GREEN}N.B. I${RESET}  You could use the scripts/tabs_to_spaces.py, and"
    echo -e "                                  scripts/normalise_json.py."
    echo -e ""
    echo -e "${GREEN}N.B. II${RESET} If you wish the tab to be ignored add 'IGNORE_TAB' on that line."
    echo -e "        For example, this '	' tab was ignored." # IGNORE_TAB
    echo -e ""
    exit 1
fi

# check that we use createObservable
if git $GIT_ARGS grep -n --column \
    -e "observable<>::create<" --and --not -e "IGNORE_OBSERVABLE_CREATE" \
    -- "*.[ch]" \
       "*[ch]pp" \
       ":(exclude,top)ext/*"
 then
   echo -e ""
   echo -e "${YELLOW}Use of rxcpp::observable<>::create<>() was found in the files above!"
   echo -e "Please use CreateObservable() from CreateObservable.hpp for more robust creation of observables.${RESET}"
   echo -e ""
   echo -e "${GREEN}N.B.${RESET} If you wish this warning to be ignored add 'IGNORE_OBSERVABLE_CREATE' on that line."
   echo -e ""
   exit 1
fi

# check for "REMOVE" that indicates debug code not suited for deployment
if git $GIT_ARGS grep -n --column \
    -e "REMOVE" --and --not -e "IGNORE_REMOVE" \
    -- "*.[ch]" \
       "*[ch]pp" \
       ":(exclude,top)ext/*"
 then
   echo -e ""
   echo -e "${YELLOW}The text \"${RED}REMOVE${YELLOW}\" was found in the files above."
   echo -e "Are you sure you intended to commit these lines? ${RESET}"
   echo -e ""
   echo -e "If so, add '${GREEN}IGNORE_REMOVE${RESET}' to those lines."
   echo -e ""
   exit 1
fi

