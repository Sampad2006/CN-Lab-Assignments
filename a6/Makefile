# Makefile for Assignment 6 Report (Cisco Packet Tracer Network Modeling)
# Course: CSE/PC/B/S/314 Computer Networks Lab
# Student: Sampad De (002410501025)

TEX = pdflatex
TEXFLAGS = -interaction=nonstopmode -halt-on-error
TARGET = report

all: $(TARGET).pdf

$(TARGET).pdf: $(TARGET).tex
	@which $(TEX) > /dev/null 2>&1 || (echo "pdflatex not found in PATH. You can compile report.tex via Overleaf or install MacTeX/TeXLive." && exit 1)
	$(TEX) $(TEXFLAGS) $(TARGET).tex
	$(TEX) $(TEXFLAGS) $(TARGET).tex

clean:
	rm -f $(TARGET).aux $(TARGET).log $(TARGET).out $(TARGET).toc $(TARGET).synctex.gz $(TARGET).fls $(TARGET).fdb_latexmk

.PHONY: all clean
