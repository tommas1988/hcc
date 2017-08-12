#! /bin/sh
cat src/comment_definitions.ini |
    sed 's/\\/\\\\/' | sed 's/\"/\\\"/' | sed "s/'/\\'/" |
    awk '
BEGIN {
        out = "static const char *comment_definitions = ";
}
{
        if ($0 != "") {
           out = out sprintf("\n  \"%s\"", $0);
        }
}
END {
        print out ";"
}
' > src/comment_definitions.c

