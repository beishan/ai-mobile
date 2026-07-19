    /* Embedded admin page revision: Chinese weather location selector. */
    .section .rodata.web_admin_html,"a",@progbits
    .balign 4
    .global web_admin_html_start
    .global web_admin_html_end
web_admin_html_start:
    .incbin "src/web/admin.html"
web_admin_html_end:
    .byte 0
