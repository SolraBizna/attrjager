Did you (or someone who wrote your [backup software](https://github.com/SolraBizna/slugger-stuff)) put `--fake-super` on the wrong end of an rsync transfer? Or maybe you have a filesystem tree with `--fake-super` metadata that you'd like to upgrade into actually-super metadata? This tool is here to help!

Build with make. (You may need to install a package with a name like `libattr1-dev` to get it to build.) Run with `--dry-run --verbose` if you want to know what it's going to do before it does it.
