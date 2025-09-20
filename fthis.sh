fthis_file_path=$1
if [ -z "$fthis_file_path" ]; then
    >&2 echo 'usage: `. path/to/fthis path/to/file/to/watch.txt`'
    return 1
fi
if [ -z "$(which wthis)" ]; then
    >&2 echo 'could not find `wthis`, please install'
    return 1;
fi
fthis_input_file=.tmp.$fthis_file_path
fthis_iteration=0
while true; do
    #{
    # move everything after execute into a new file:
    sed -e '1,/# ⬇EXECUTE⬇/ d' $fthis_file_path > $fthis_input_file
    if [[ -z $(grep '[^[:space:]]' $fthis_input_file) ]]; then
        #{
        >&2 echo "nothing to execute"
        last_line=$(tail -c 17 $fthis_file_path)
        if [ "$last_line" != "
# ⬇EXECUTE⬇" ]; then
            echo -e "# ⬇EXECUTE⬇" >> $fthis_file_path
        else
            >&2 echo "already have EXECUTE ready at last line"
        fi
        #}
    else
        #{
        # delete everything after execute, including execute:
        sed -i '/^# ⬇EXECUTE⬇/Q' $fthis_file_path
        # display input:
        echo -e "# ${fthis_iteration} input:" > >(tee -a $fthis_file_path >&2)
        cat $fthis_input_file > >(tee -a $fthis_file_path >&2)
        # display output:
        echo -e "# ${fthis_iteration} output:" > >(tee -a $fthis_file_path >&2)
        # tee tmp stdout to file and to our stderr
        # tee tmp stderr to file and to our stderr
        source $fthis_input_file > >(tee -a $fthis_file_path >&2) 2> >(tee -a $fthis_file_path >&2)
        # reset file for next iteration:
        echo -e "\n# ⬇EXECUTE⬇" > >(tee -a $fthis_file_path >&2)
        ((++fthis_iteration))
        #}
    fi
    rm $fthis_input_file
    wthis $fthis_file_path
    #}
done
