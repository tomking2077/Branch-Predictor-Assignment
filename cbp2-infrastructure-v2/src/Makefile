CXX		=	g++
CXXFLAGS	=	-g -O3 -Wall

all:		predict predict_extra_credit appendix_b_check

predict:	predict.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o predict predict.cc trace.cc

predict_extra_credit:        predict_extra_credit.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o predict_extra_credit predict_extra_credit.cc trace.cc

appendix_b_check:	appendix_b_check.cc predictor.h branch.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o appendix_b_check appendix_b_check.cc

clean:
		rm -f predict predict_extra_credit appendix_b_check
