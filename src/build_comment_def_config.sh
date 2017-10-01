#! /bin/sh
cat comment_def_config.ini |
    sed 's/\\/\\\\/' | sed 's/\"/\\\"/' | sed "s/'/\\'/" |
    awk '
BEGIN {
        out = "static const char *comment_def_config = ";
}
{
        if ($0 != "") {
           out = out sprintf("\n  \"%s\"", $0);
        }
}
END {
        print out ";"
}
' > comment_def_config.c

