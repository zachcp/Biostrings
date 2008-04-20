#include <float.h>
#include "Biostrings.h"

#define MAX(x, y) (x > y ? x : y)

#define NEGATIVE_INFINITY (- FLT_MAX)

#define  GLOBAL_ALIGNMENT 1
#define   LOCAL_ALIGNMENT 2
#define OVERLAP_ALIGNMENT 3

#define REPLACEMENT 'R'
#define INSERTION   'I'
#define DELETION    'D'

#define F_MATRIX(i, j) (fMatrix[(nCharString2 + 1) * i + j])
#define H_MATRIX(i, j) (hMatrix[(nCharString2 + 1) * i + j])
#define V_MATRIX(i, j) (vMatrix[(nCharString2 + 1) * i + j])
#define TRACE_MATRIX(i, j) (traceMatrix[(nCharString2 + 1) * i + j])

#define SAFE_SUM(x, y) (x == NEGATIVE_INFINITY ? x : (x + y))

#define SET_LOOKUP_VALUE(lookupTable, length, key) \
{ \
	unsigned char lookupKey = (unsigned char) (key); \
	if (lookupKey >= (length) || (lookupValue = (lookupTable)[lookupKey]) == NA_INTEGER) { \
		error("key %d not in lookup table", (int) lookupKey); \
	} \
}

static int nCharAligned = 0;
static char *align1Buffer, *align2Buffer, *align1, *align2;

/* Returns the score of the alignment */
static float pairwiseAlignment(
		RoSeq stringElements1,
		RoSeq stringElements2,
		const double *qualityElements1,
		const double *qualityElements2,
		int qualityElements1Length,
		int qualityElements2Length,
		char gapCode,
		int typeCode,
		const int *lookupTable,
		int lookupTableLength,
		const double *matchScoring,
		const int *matchScoringDim,
		double gapOpening,
		double gapExtension)
{
	int i, j, iMinus1, jMinus1;

	/* Step 1:  Get information on input strings */
	int nCharString1, nCharString2;
	nCharString1 = stringElements1.nelt;
	nCharString2 = stringElements2.nelt;

	/* Step 2:  Create objects for scores and traceback values */
	float *fMatrix, *hMatrix, *vMatrix;
	fMatrix = (float *) R_alloc((long) (nCharString1 + 1) * (nCharString2 + 1), sizeof(float));
	if (gapOpening == 0) {
		hMatrix = fMatrix;
		vMatrix = fMatrix;
	} else {
		hMatrix = (float *) R_alloc((long) (nCharString1 + 1) * (nCharString2 + 1), sizeof(float));
		vMatrix = (float *) R_alloc((long) (nCharString1 + 1) * (nCharString2 + 1), sizeof(float));		
	}
	if (typeCode == GLOBAL_ALIGNMENT) {
		for (i = 0; i <= nCharString1; i++)
			H_MATRIX(i, 0) = gapOpening + i * gapExtension;
		for (j = 0; j <= nCharString2; j++)
			V_MATRIX(0, j) = gapOpening + j * gapExtension;
	} else if (typeCode == LOCAL_ALIGNMENT || typeCode == OVERLAP_ALIGNMENT) {
		for (i = 0; i <= nCharString1; i++)
			H_MATRIX(i, 0) = 0;
		for (j = 0; j <= nCharString2; j++)
			V_MATRIX(0, j) = 0;
	}
	if (gapOpening < 0) {
		F_MATRIX(0, 0) = 0;
		for (i = 1; i <= nCharString1; i++) {
			F_MATRIX(i, 0) = NEGATIVE_INFINITY;
			V_MATRIX(i, 0) = NEGATIVE_INFINITY;
		}
		for (j = 1; j <= nCharString2; j++) {
			F_MATRIX(0, j) = NEGATIVE_INFINITY;
			H_MATRIX(0, j) = NEGATIVE_INFINITY;
		}
	}
	char *traceMatrix = (char *) R_alloc((long) (nCharString1 + 1) * (nCharString2 + 1), sizeof(char));

	/* Step 3:  Generate scores and traceback values */
	int startRow = -1, startCol = -1;
	float score, startScore = NEGATIVE_INFINITY;
	if (gapOpening == 0) {
		for (i = 1, iMinus1 = 0; i <= nCharString1; i++, iMinus1++) {
			for (j = 1, jMinus1 = 0; j <= nCharString2; j++, jMinus1++) {
				int lookupValue;
				SET_LOOKUP_VALUE(lookupTable, lookupTableLength, stringElements1.elts[iMinus1]);
				int element1 = lookupValue;
				SET_LOOKUP_VALUE(lookupTable, lookupTableLength, stringElements2.elts[jMinus1]);
				int element2 = lookupValue;
				float scoreReplacement, scoreInsertion, scoreDeletion;
				double quality1 = (qualityElements1Length == 1 ? qualityElements1[0] : qualityElements1[iMinus1]);
				double quality2 = (qualityElements2Length == 1 ? qualityElements2[0] : qualityElements2[jMinus1]);
				double quality = quality1 * quality2;
				scoreReplacement =
					F_MATRIX(iMinus1, jMinus1) + (float) (quality * matchScoring[matchScoringDim[0] * element1 + element2]);
				scoreInsertion   = V_MATRIX(i, jMinus1) + gapExtension;
				scoreDeletion    = H_MATRIX(iMinus1, j) + gapExtension;
				if (typeCode == LOCAL_ALIGNMENT) {
					if (scoreReplacement < 0)
						scoreReplacement = 0;
					if (scoreReplacement > startScore) {
						startRow = i;
						startCol = j;
						startScore = scoreReplacement;
					}
				}
				if (scoreReplacement >= MAX(scoreInsertion, scoreDeletion)) {
					F_MATRIX(i, j) = scoreReplacement;
					TRACE_MATRIX(i, j) = REPLACEMENT;
				} else if (scoreInsertion >= scoreDeletion) {
					F_MATRIX(i, j) = scoreInsertion;
					TRACE_MATRIX(i, j) = INSERTION;
				} else {
					F_MATRIX(i, j) = scoreDeletion;
					TRACE_MATRIX(i, j) = DELETION;
				}
			}
		}
	} else {
		for (i = 1, iMinus1 = 0; i <= nCharString1; i++, iMinus1++) {
			for (j = 1, jMinus1 = 0; j <= nCharString2; j++, jMinus1++) {
				int lookupValue;
				SET_LOOKUP_VALUE(lookupTable, lookupTableLength, stringElements1.elts[iMinus1]);
				int element1 = lookupValue;
				SET_LOOKUP_VALUE(lookupTable, lookupTableLength, stringElements2.elts[jMinus1]);
				int element2 = lookupValue;
				float scoreReplacement, scoreInsertion, scoreDeletion;
				double quality1 = (qualityElements1Length == 1 ? qualityElements1[0] : qualityElements1[iMinus1]);
				double quality2 = (qualityElements2Length == 1 ? qualityElements2[0] : qualityElements2[jMinus1]);
				double quality = quality1 * quality2;
				F_MATRIX(i, j) =
					SAFE_SUM(MAX(F_MATRIX(iMinus1, jMinus1), MAX(H_MATRIX(iMinus1, jMinus1), V_MATRIX(iMinus1, jMinus1))),
					         (float) (quality * matchScoring[matchScoringDim[0] * element1 + element2]));
				if (typeCode == LOCAL_ALIGNMENT) {
					if (F_MATRIX(i, j) < 0)
						F_MATRIX(i, j) = 0;
					if (F_MATRIX(i, j) > startScore) {
						startRow = i;
						startCol = j;
						startScore = F_MATRIX(i, j);
					}
				}
				H_MATRIX(i, j) = 
					MAX(SAFE_SUM(F_MATRIX(iMinus1, j), gapOpening + gapExtension),
					    SAFE_SUM(H_MATRIX(iMinus1, j), gapExtension));
				V_MATRIX(i, j) =
					MAX(SAFE_SUM(F_MATRIX(i, jMinus1), gapOpening + gapExtension),
					    SAFE_SUM(V_MATRIX(i, jMinus1), gapExtension));

				scoreReplacement = F_MATRIX(i, j);
				scoreInsertion   = V_MATRIX(i, j);
				scoreDeletion    = H_MATRIX(i, j);

				if (scoreReplacement >= MAX(scoreInsertion, scoreDeletion))
					TRACE_MATRIX(i, j) = REPLACEMENT;
				else if (scoreInsertion >= scoreDeletion)
					TRACE_MATRIX(i, j) = INSERTION;
				else
					TRACE_MATRIX(i, j) = DELETION;
			}
		}
	}
	if (typeCode == GLOBAL_ALIGNMENT) {
		startRow = nCharString1;
		startCol = nCharString2;
		startScore =
			MAX(F_MATRIX(nCharString1, nCharString2),
			MAX(H_MATRIX(nCharString1, nCharString2),
			    V_MATRIX(nCharString1, nCharString2)));
	} else if (typeCode == OVERLAP_ALIGNMENT) {
		for (i = 1; i <= nCharString1; i++) {
			score =
				MAX(F_MATRIX(i, nCharString2),
				MAX(H_MATRIX(i, nCharString2),
				    V_MATRIX(i, nCharString2)));
			if (score > startScore) {
				startRow = i;
				startCol = nCharString2;
				startScore = score;
			}
	    }
		for (j = 1; j <= nCharString2; j++) {
			score =
				MAX(F_MATRIX(nCharString1, j),
				MAX(H_MATRIX(nCharString1, j),
				    V_MATRIX(nCharString1, j)));
			if (score > startScore) {
				startRow = nCharString1;
				startCol = j;
				startScore = score;
			}
	    }
	}

	/* Step 4:  Get a starting location for the traceback */
	nCharAligned = 0;
	int alignmentBufferSize = nCharString1 + nCharString2;
	align1Buffer = (char *) R_alloc((long) alignmentBufferSize, sizeof(char));
	align2Buffer = (char *) R_alloc((long) alignmentBufferSize, sizeof(char));
	align1 = align1Buffer + alignmentBufferSize;
	align2 = align2Buffer + alignmentBufferSize;
	if (typeCode == OVERLAP_ALIGNMENT && (startRow < nCharString1 || startCol < nCharString2)) {
		if (startRow == nCharString1) {
			nCharAligned += nCharString2 - startCol;
			for (j = 1; j <= nCharString2 - startCol; j++) {
				align1--;
				align2--;
				*align1 = gapCode;
				*align2 = stringElements2.elts[nCharString2 - j];
			}
	    } else {
			nCharAligned += nCharString1 - startRow;
			for (i = 1; i <= nCharString1 - startRow; i++) {
				align1--;
				align2--;
				*align1 = stringElements1.elts[nCharString1 - i];
				*align2 = gapCode;
			}
	    }
	}

	/* Step 5:  Traceback through the score matrix */
	i = startRow;
	j = startCol;
	while (((typeCode == GLOBAL_ALIGNMENT || typeCode == OVERLAP_ALIGNMENT) && (i >= 1 || j >= 1)) ||
			(typeCode == LOCAL_ALIGNMENT && F_MATRIX(i, j) > 0)) {
		nCharAligned++;
		align1--;
		align2--;
		iMinus1 = i - 1;
		jMinus1 = j - 1;
		char traceValue;
		if (j == 0)
			traceValue = DELETION;
		else if (i == 0)
			traceValue = INSERTION;
		else
			traceValue = TRACE_MATRIX(i, j);
		switch (traceValue) {
		    case DELETION:
			*align1 = stringElements1.elts[iMinus1];
			*align2 = gapCode;
			i--;
			break;
		    case INSERTION:
			*align1 = gapCode;
			*align2 = stringElements2.elts[jMinus1];
			j--;
			break;
		    case REPLACEMENT:
			*align1 = stringElements1.elts[iMinus1];
			*align2 = stringElements2.elts[jMinus1];
			i--;
			j--;
			break;
		    default:
			error("unknown traceback code %d", traceValue);
			break;
		}
	}

	return startScore;
}

/*
 * INPUTS
 * 'string1', 'string2':  left and right XString objects
 * 'quality1', 'quality2': left and right quality measures between 0 and 1
 * 'gapCode':  encoded value of the '-' letter (raw vector of length 1)
 * 'typeCode':  type of pairwise alignment
 *          (integer vector of length 1; 1 = 'global', 2 = 'local', 3 = 'overlap')
 * 'lookupTable':  lookup table for translating XString bytes to scoring matrix
 *                 indices (integer vector)
 * 'matchScoring':  scoring matrix for matches/mismatches (double matrix)
 * 'matchScoringDim':  dimension of 'matchScoring' (integer vector of length 2
 * 'gapOpening':  gap opening cost or penalty (double vector of length 1)
 * 'gapExtension':  gap extension cost or penalty (double vector of length 1)
 * 
 * OUTPUT
 * Return a named list with 3 elements: 2 "externalptr" objects describing
 * the alignments and 1 integer vector containing the alignment score.
 * Note that the 2 XString objects to align should contain no gaps.
 */
SEXP align_pairwiseAlignment(
		SEXP string1,
		SEXP string2,
		SEXP quality1,
		SEXP quality2,
		SEXP gapCode,
		SEXP typeCode,
		SEXP lookupTable,
		SEXP matchScoring,
		SEXP matchScoringDim,
		SEXP gapOpening,
		SEXP gapExtension)
{
	float score;
	RoSeq stringElements1, stringElements2;
	SEXP answer, answerNames, answerElements, tag;

	stringElements1 = _get_XString_asRoSeq(string1);
	stringElements2 = _get_XString_asRoSeq(string2);
	score = pairwiseAlignment(
			stringElements1,
			stringElements2,
			REAL(quality1),
			REAL(quality2),
			LENGTH(quality1),
			LENGTH(quality2),
			(char) RAW(gapCode)[0],
			INTEGER(typeCode)[0],
			INTEGER(lookupTable),
			LENGTH(lookupTable),
			REAL(matchScoring),
			INTEGER(matchScoringDim),
			REAL(gapOpening)[0],
			REAL(gapExtension)[0]);

	PROTECT(answer = NEW_LIST(3));
	/* set the names */
	PROTECT(answerNames = NEW_CHARACTER(3));
	SET_STRING_ELT(answerNames, 0, mkChar("align1"));
	SET_STRING_ELT(answerNames, 1, mkChar("align2"));
	SET_STRING_ELT(answerNames, 2, mkChar("score"));
	SET_NAMES(answer, answerNames);
	UNPROTECT(1);
	/* set the "align1" element */
	PROTECT(tag = NEW_RAW(nCharAligned));
	memcpy((char *) RAW(tag), align1, nCharAligned * sizeof(char));
	PROTECT(answerElements = _new_XRaw(tag));
	SET_ELEMENT(answer, 0, answerElements);
	UNPROTECT(2);
	/* set the "align2" element */
	PROTECT(tag = NEW_RAW(nCharAligned));
	memcpy((char *) RAW(tag), align2, nCharAligned * sizeof(char));
	PROTECT(answerElements = _new_XRaw(tag));
	SET_ELEMENT(answer, 1, answerElements);
	UNPROTECT(2);
	/* set the "score" element */
	PROTECT(answerElements = NEW_NUMERIC(1));
	REAL(answerElements)[0] = score;
	SET_ELEMENT(answer, 2, answerElements);
	UNPROTECT(1);
	/* answer is ready */
	UNPROTECT(1);
	return answer;
}
