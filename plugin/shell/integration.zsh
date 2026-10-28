# Source explicitly in an interactive Zsh. No startup files are edited.
[[ -n ${ZSH_VERSION-} && -o interactive ]] || return 0

_command_queue_preexec() {
    printf '\e]777;command-queue;1;running\a'
}
autoload -Uz add-zsh-hook
add-zsh-hook preexec _command_queue_preexec

_command_queue_install_hooks() {
    local primary=$'%{\e]777;command-queue;1;prompt\a%}'
    local continuation=$'%{\e]777;command-queue;1;continuation\a%}'
    [[ $PS1 == *"$primary"* ]] || PS1+=$primary
    [[ $PS2 == *"$continuation"* ]] || PS2+=$continuation
}
_command_queue_install_hooks
unfunction _command_queue_install_hooks
