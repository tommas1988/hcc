#! /bin/sh
cat default_comment_defs.ini |
    sed 's/\\/\\\\/' | sed 's/\"/\\\"/' | sed "s/'/\\'/" |
    awk '
BEGIN {
        out = "static const char *comment_defs_string = ";
}
{
        if ($0 != "") {
           out = out sprintf("\n  \"%s\\n\"", $0);
        }
}
END {
        print out ";"
}
' > comment_defs_string.c

