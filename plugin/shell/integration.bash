# Source explicitly in an interactive Bash 4.4+ shell. Does not install itself.
# PS0 reports accepted commands; prompt suffixes report primary/continuation
# prompts without replacing existing prompts, PROMPT_COMMAND, or DEBUG traps.
if [[ -z ${BASH_VERSION-} || $- != *i* ]]; then
    return 0
fi
if (( BASH_VERSINFO[0] < 4 || (BASH_VERSINFO[0] == 4 && BASH_VERSINFO[1] < 4) )); then
    printf 'Command Queue hooks need Bash 4.4 or newer.\n' >&2
    return 1
fi

_command_queue_install_hooks() {
    local primary=$'\[\e]777;command-queue;1;prompt\a\]'
    local continuation=$'\[\e]777;command-queue;1;continuation\a\]'
    local running=$'\e]777;command-queue;1;running\a'
    [[ ${PS1-} == *"$primary"* ]] || PS1+=$primary
    [[ ${PS2-} == *"$continuation"* ]] || PS2+=$continuation
    [[ ${PS0-} == *"$running"* ]] || PS0+=$running
}
_command_queue_install_hooks
unset -f _command_queue_install_hooks
