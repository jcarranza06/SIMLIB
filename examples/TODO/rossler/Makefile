
NAME    = rossler
CFLAGS  = -O2
LDLIBS = -lm

all: pdf
$(NAME): $(NAME).c

html: $(NAME).c
	vim -c "set background=dark" -c "TOhtml" -c "wqa" $^

run: $(NAME).dat
$(NAME).dat: $(NAME)
	./$(NAME) >"$(NAME).dat"

plot: $(NAME).pdf
pdf: $(NAME).pdf
$(NAME).pdf: $(NAME).dat $(NAME).plt
	gnuplot $(NAME).plt

tikz: $(NAME).tex
$(NAME).tex: $(NAME).dat $(NAME)-tikz.plt
	gnuplot $(NAME)-tikz.plt

clean:
	rm -f $(NAME) $(NAME).dat $(NAME).pdf $(NAME).c.html $(NAME).tex

pack: clean
	(N=`basename $$(pwd)`; cd ..; \
	tar czvf $$N-`date +"%Y%m%d"`.tar.gz $$N ; \
	echo "Done.")

