"""Reproduce el orden de párrafos, tablas y figuras del Word en PDF."""
from io import BytesIO
from pathlib import Path
from xml.sax.saxutils import escape
from docx import Document
from docx.document import Document as DocumentClass
from docx.table import Table as DocxTable
from docx.text.paragraph import Paragraph as DocxParagraph
from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Table, TableStyle, Image, Spacer, KeepTogether

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'documentacion' / 'informe_final.docx'
TARGET = ROOT / 'documentacion' / 'informe_final.pdf'
doc = Document(SOURCE)
pdfmetrics.registerFont(TTFont('Arial', r'C:\Windows\Fonts\arial.ttf'))
pdfmetrics.registerFont(TTFont('Arial-Bold', r'C:\Windows\Fonts\arialbd.ttf'))
sheet = getSampleStyleSheet()
sheet.add(ParagraphStyle(name='BodyCustom', fontName='Arial', fontSize=9.2, leading=12.6,
                         spaceAfter=6, textColor=colors.black))
sheet.add(ParagraphStyle(name='TitleCustom', fontName='Arial-Bold', fontSize=16, leading=20,
                         spaceAfter=13, textColor=colors.black))
sheet.add(ParagraphStyle(name='HeadingCustom', fontName='Arial-Bold', fontSize=11.6, leading=15,
                         spaceBefore=10, spaceAfter=5, keepWithNext=True, textColor=colors.black))
sheet.add(ParagraphStyle(name='CaptionCustom', fontName='Arial', fontSize=8.5, leading=11,
                         spaceBefore=2, spaceAfter=10, alignment=TA_CENTER, textColor=colors.black))
sheet.add(ParagraphStyle(name='CellCustom', fontName='Arial', fontSize=7.2, leading=9.3,
                         textColor=colors.black))
sheet.add(ParagraphStyle(name='CellHeadCustom', fontName='Arial-Bold', fontSize=7.2, leading=9.3,
                         textColor=colors.black))


def para(text, style):
    return Paragraph(escape(text).replace('\n', '<br/>'), sheet[style])


story = []
pending_image = None
for block in doc.iter_inner_content():
    if isinstance(block, DocxParagraph):
        blips = block._p.xpath('.//a:blip')
        if blips:
            for blip in blips:
                rid = blip.get('{http://schemas.openxmlformats.org/officeDocument/2006/relationships}embed')
                data = doc.part.related_parts[rid].blob
                with PILImage.open(BytesIO(data)) as source:
                    ratio = source.height / source.width
                width = 15.4 * cm
                pending_image = Image(BytesIO(data), width=width, height=width * ratio)
            continue
        if not block.text.strip():
            continue
        style = ('TitleCustom' if block.style.name == 'Title' else
                 'HeadingCustom' if block.style.name.startswith('Heading') else
                 'CaptionCustom' if block.style.name == 'Caption' else 'BodyCustom')
        if pending_image is not None and style == 'CaptionCustom':
            story.append(KeepTogether([pending_image, para(block.text, style)]))
            pending_image = None
        else:
            story.append(para(block.text, style))
    elif isinstance(block, DocxTable):
        data = []
        for i, row in enumerate(block.rows):
            style = 'CellHeadCustom' if i == 0 else 'CellCustom'
            data.append([para(cell.text, style) for cell in row.cells])
        widths = ([2.0, 1.3, 1.0, 1.9, 1.1, 1.1, 2.0, 1.7, 1.4]
                  if len(block.columns) == 9 else [2.6, 2.9, 2.9, 2.9, 2.9])
        item = Table(data, colWidths=[x * cm for x in widths], repeatRows=1, hAlign='CENTER')
        item.setStyle(TableStyle([
            ('GRID', (0, 0), (-1, -1), .35, colors.black),
            ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
            ('LEFTPADDING', (0, 0), (-1, -1), 4),
            ('RIGHTPADDING', (0, 0), (-1, -1), 4),
            ('TOPPADDING', (0, 0), (-1, -1), 3),
            ('BOTTOMPADDING', (0, 0), (-1, -1), 3),
        ]))
        story.append(item)
        story.append(Spacer(1, 5))


def footer(canvas, document):
    canvas.saveState()
    canvas.setFont('Arial', 8)
    canvas.drawRightString(A4[0] - 2.4 * cm, 1.4 * cm, str(document.page))
    canvas.restoreState()


pdf = SimpleDocTemplate(str(TARGET), pagesize=A4, leftMargin=2.4*cm, rightMargin=2.4*cm,
                        topMargin=2.0*cm, bottomMargin=2.0*cm,
                        title='Generación de imágenes cartoon con MPI y OpenMP')
pdf.build(story, onFirstPage=footer, onLaterPages=footer)
print(TARGET)
