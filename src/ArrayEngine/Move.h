#pragma once

struct Move {
	int fromRow;
	int fromCol;
	int toRow;
	int toCol;

	Move() : fromRow(0), fromCol(0), toRow(0), toCol(0) {}

	Move(int fr, int fc, int tr, int tc)
		: fromRow(fr), fromCol(fc), toRow(tr), toCol(tc) {}
	};