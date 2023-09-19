
NAME=SIMLIB-doc

$(NAME).pdf: $(NAME).tex
	pdflatex $(NAME).tex
	pdflatex $(NAME).tex
	pdflatex $(NAME).tex

clean:
	rm -f *.log *.aux *.toc

clean-all: clean
	rm -f $(NAME).pdf

