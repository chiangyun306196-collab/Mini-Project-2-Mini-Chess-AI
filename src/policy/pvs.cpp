#include <utility>
#include <algorithm> 
#include <vector>
#include "state.hpp"
#include "pvs.hpp"
/*============================================================
 * PVS — quiescence
 *============================================================*/
int PVS::quiescence(
            State *state,
            int alpha,
            int beta, 
            int ply, 
            SearchContext& ctx
        ){
            ctx.nodes++;
            if(ctx.stop){
                return 0;
            }

            int s = state->evaluate(true,true,nullptr);//
            if(s >= beta){
                return beta;
            }

            if(s > alpha){
                alpha = s;
            }

            if (state->legal_actions.empty() && state->game_state == UNKNOWN) {
                state->get_legal_actions();
            }

            for(auto& action:state->legal_actions){
                int r = action.second.first; //終點(r,c)
                int c = action.second.second;

                if(state->board.board[0][r][c] == 0 && state->board.board[1][r][c] == 0){
                    continue;
                }

                State* nex = state->next_state(action);
                int same = nex->same_player_as_parent();
                int scor;

                if(same){
                    scor = quiescence(nex,alpha,beta,ply+1,ctx);
                }else{
                    scor = -quiescence(nex,-beta,-alpha,ply + 1,ctx);
                }

                delete nex;

                if(scor >= beta){
                    return beta;
                }
                if(scor >= alpha){
                    alpha = scor;
                }
            }
            return alpha;
        }


/*============================================================
 * PVS — eval_ctx
 *============================================================*/
int PVS::eval_ctx(
    State *state,
    int depth,
    int alpha,
    int beta,
    GameHistory& history,
    int ply,
    SearchContext& ctx,
    const MMParams& p
){
    ctx.nodes++;
    if(ply > ctx.seldepth){
        ctx.seldepth = ply;
    }
    if(ctx.stop){
        return 0;
    }

    if(state->legal_actions.empty() && state->game_state == UNKNOWN){
        state->get_legal_actions();
    }

    if(state->game_state == WIN){
        return P_MAX - ply;
    }

    if(state->game_state == DRAW){
        return 0;
    }

    int rep_score;
    if(state->check_repetition(history, rep_score)){
        return rep_score;
    }
    
    history.push(state->hash());

    if(depth <= 0){
        int score = quiescence(state,alpha,beta,ply,ctx); 
        history.pop(state->hash());
        return score;
    }

    struct ScoredAction {
        Move action;
        int w;
    };

    std::vector<ScoredAction> scored_actions;
    scored_actions.reserve(state->legal_actions.size());

    for(const auto& action : state->legal_actions){
        int w = 0;
        int r = action.second.first; //終點座標x
        int c = action.second.second;//終點座標y
        if(state->board.board[0][r][c] != 0 || state->board.board[1][r][c] != 0){
            w += 100;
        }
        if(r >= 2 && r <= 3 && c >= 2 && c <= 3){
            w += 10;
        }
        scored_actions.push_back({action, w});
    }

    std::sort(
                scored_actions.begin(),
                scored_actions.end(),
                [](const ScoredAction& a,const ScoredAction& b){
                    return a.w > b.w;
                }
            );
    
    int i; 
    for(i=0;i < state->legal_actions.size();++i){
        state->legal_actions[i] = scored_actions[i].action;
    }

    int best_score = M_MAX;
    bool is_fir = true;

    for(auto& action : state->legal_actions){
        State* next = state->next_state(action);
        bool same = next->same_player_as_parent();
        int s;
        if(is_fir){
            if(same){
                s = eval_ctx(next, depth - 1, alpha, beta, history, ply + 1, ctx, p);
            }else{
                s = -eval_ctx(next, depth - 1, -beta, -alpha, history, ply + 1, ctx, p);
            }
            is_fir = false;
        }else{
             if(same){
                s = eval_ctx(next, depth - 1, alpha, alpha + 1, history, ply + 1, ctx, p);
            }else{
                s = -eval_ctx(next, depth - 1, -alpha - 1, -alpha, history, ply + 1, ctx, p);
            }

            if(s > alpha && s < beta){
                if(same){
                    s = eval_ctx(next, depth - 1, s, beta, history, ply + 1, ctx, p);
                } else {
                    s = -eval_ctx(next, depth - 1, -beta, -s, history, ply + 1, ctx, p);
                }
            }
        }

        delete next;

        if(s > best_score){
            best_score = s;
        }
        if(best_score > alpha){
            alpha = best_score;
        }
        if(alpha >= beta){
            break;
        }
    }

    history.pop(state->hash());
    return best_score;
}


/*============================================================
 * PVS — search
 *============================================================*/
SearchResult PVS::search(
    State *state,
    int depth,
    GameHistory& history,
    SearchContext& ctx
){
    ctx.reset();
    MMParams p = MMParams::from_map(ctx.params);
    SearchResult result;
    result.depth = depth;

    if(!state->legal_actions.size()){
        state->get_legal_actions();
    }

    if(!state->legal_actions.empty()){
        result.best_move = state->legal_actions[0];
    }

    struct RootMove{
        Move action;
        int w;
    };

    std::vector<RootMove> root_moves;
    root_moves.reserve(state->legal_actions.size());

    for(const auto& action : state->legal_actions){
        int w = 0;
        int r = action.second.first;
        int c = action.second.second;

        if(state->board.board[0][r][c] != 0 || state->board.board[1][r][c]){
            w += 100;
        }

        if(r >= 2 && r <= 3 && c >= 2 && c <= 3){
            w += 10;
        }
        root_moves.push_back({action, w});

    }

    std::sort(
        root_moves.begin(),
        root_moves.end(),
        [](const RootMove& a, const RootMove& b){
            return a.w > b.w;
        }
    );

    int alpha = M_MAX;
    int beta = P_MAX;
    int move_index = 0;
    int total_moves = (int)state->legal_actions.size();

    for(auto& action :root_moves){
        State* next = state->next_state(action.action);
        bool same = next->same_player_as_parent();
        int raw;
        if(same){
            raw = eval_ctx(
                next,
                depth - 1,
                alpha,
                beta,
                history,
                1,
                ctx,
                p
            );
        }else{
            raw = eval_ctx(
                next,
                depth - 1,
                -beta,
                -alpha,
                history,
                1,
                ctx,
                p
            );
        }
        int score = same ? raw : -raw;
        delete next;

        if(score > alpha){
            alpha = score;
            result.best_move = action.action;

            if(p.report_partial && ctx.on_root_update){
               ctx.on_root_update({result.best_move, alpha, depth, move_index + 1, total_moves});
            }
        }  
        move_index++;
    }

    result.score = alpha;
    result.seldepth = ctx.seldepth;
    result.nodes = ctx.nodes;

    return result;
}


/*============================================================
* PVS — default_params / param_defs
 *============================================================*/
ParamMap PVS::default_params(){
    return {
        {"UseKPEval", "true"},
        {"UseEvalMobility", "true"},
        {"ReportPartial", "true"},
    };
}

std::vector<ParamDef> PVS::param_defs(){
    return {
        {"UseKPEval", ParamDef::CHECK, "true"},
        {"UseEvalMobility", ParamDef::CHECK, "true"},
        {"ReportPartial", ParamDef::CHECK, "true"},
    };
}
