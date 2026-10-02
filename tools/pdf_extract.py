"""Create searchable, page-addressable text without changing source PDFs."""
import argparse
import hashlib
import json
from pathlib import Path

import pymupdf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdf", nargs="+", type=Path)
    parser.add_argument("--out", type=Path, default=Path("artifacts/pdf-text"))
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    for source in args.pdf:
        with pymupdf.open(source) as doc:
            pages = [page.get_text(sort=True) for page in doc]
            record = {
                "source": source.as_posix(),
                "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "pdf_pages": len(pages),
                "metadata": doc.metadata,
                "toc": doc.get_toc(),
                "pages": [
                    {"pdf_page": number, "text": content}
                    for number, content in enumerate(pages, 1)
                ],
            }
        destination = args.out / source.stem
        destination.with_suffix(".json").write_text(
            json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        destination.with_suffix(".txt").write_text(
            "\n\n".join(
                f"=== PDF PAGE {number} ===\n{content}"
                for number, content in enumerate(pages, 1)
            ), encoding="utf-8"
        )
        print(json.dumps({key: record[key] for key in ("source", "sha256", "pdf_pages")}))


if __name__ == "__main__":
    main()
